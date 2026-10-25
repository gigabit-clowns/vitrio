// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_array_layout.hpp"

#include "blosc_zarr_bytes_codec.hpp"
#include "crc32c_zarr_bytes_codec.hpp"
#include "gzip_zarr_bytes_codec.hpp"
#include "zarr_data_type.hpp"
#include "zarr_fill_value.hpp"
#include "zarr_json.hpp"
#include "zlib_zarr_bytes_codec.hpp"
#include "zstd_zarr_bytes_codec.hpp"

#include <algorithm>
#include <memory>
#include <numeric>
#include <stdexcept>

namespace vitrio
{
namespace zarr
{

namespace
{

using codec_list = std::vector<std::shared_ptr<const zarr_bytes_codec>>;

// The members of the metadata of an array of version 3. Any other must be an
// extension that says it need not be understood.
const char *const v3_members[] = {
	"zarr_format", "node_type", "shape", "data_type", "chunk_grid",
	"chunk_key_encoding", "fill_value", "codecs", "dimension_names",
	"storage_transformers",
};

std::vector<std::size_t> get_chunk_extents(
	const nlohmann::json &value,
	std::size_t rank,
	const std::string &where
)
{
	auto extents = get_extents(value, "chunk shape", where);
	if (extents.size() != rank)
	{
		reject_metadata(
			where, "The chunks do not have as many axes as the array.");
	}
	if (
		std::find(extents.cbegin(), extents.cend(), std::size_t(0)) !=
		extents.cend()
	)
	{
		reject_metadata(where, "A chunk is empty along an axis.");
	}

	return extents;
}

// A codec of version 2, its "id" one of numcodecs.
std::shared_ptr<const zarr_bytes_codec> make_v2_codec(
	const nlohmann::json &compressor,
	const std::string &where
)
{
	const auto &id = get_member(compressor, "id", where);
	if (id == "blosc")
	{
		return std::make_shared<blosc_zarr_bytes_codec>();
	}
	if (id == "zstd")
	{
		return std::make_shared<zstd_zarr_bytes_codec>();
	}
	if (id == "zlib")
	{
		return std::make_shared<zlib_zarr_bytes_codec>();
	}
	if (id == "gzip")
	{
		return std::make_shared<gzip_zarr_bytes_codec>();
	}

	refuse_metadata(where, "Chunks compressed with " + id.dump() +
		" are not read.");
}

// A codec of version 3, an object that names it, or its name alone.
const nlohmann::json& get_codec_name(
	const nlohmann::json &codec,
	const std::string &where
)
{
	const auto &name =
		codec.is_string() ? codec : get_member(codec, "name", where);
	if (!name.is_string())
	{
		reject_metadata(where, "A codec is not named.");
	}

	return name;
}

const nlohmann::json& get_configuration(const nlohmann::json &codec)
{
	static const nlohmann::json none = nlohmann::json::object();
	if (!codec.is_object())
	{
		return none;
	}

	const auto found = codec.find("configuration");
	return found == codec.end() ? none : *found;
}

std::shared_ptr<const zarr_bytes_codec> make_v3_bytes_codec(
	const std::string &name
)
{
	if (name == "blosc")
	{
		return std::make_shared<blosc_zarr_bytes_codec>();
	}
	if (name == "zstd")
	{
		return std::make_shared<zstd_zarr_bytes_codec>();
	}
	if (name == "gzip")
	{
		return std::make_shared<gzip_zarr_bytes_codec>();
	}
	if (name == "crc32c")
	{
		return std::make_shared<crc32c_zarr_bytes_codec>();
	}

	return nullptr;
}

// Applies a transpose to the order the axes are in so far: the axis at each
// position after it is the one at the position its order names before it.
void apply_transpose(
	const nlohmann::json &configuration,
	std::vector<std::size_t> &axes,
	const std::string &where
)
{
	const auto found = configuration.find("order");
	if (found == configuration.end() || !found->is_array())
	{
		reject_metadata(where, "A transpose states no order of the axes.");
	}

	const auto order = get_extents(*found, "order of a transpose", where);
	auto sorted = order;
	std::sort(sorted.begin(), sorted.end());
	std::vector<std::size_t> identity(axes.size());
	std::iota(identity.begin(), identity.end(), std::size_t(0));
	if (sorted != identity)
	{
		reject_metadata(
			where,
			"The order of a transpose is not a permutation of the axes."
		);
	}

	std::vector<std::size_t> transposed(axes.size());
	for (std::size_t position = 0; position < axes.size(); ++position)
	{
		transposed[position] = axes[order[position]];
	}
	axes = transposed;
}

byte_order parse_endian(
	const nlohmann::json &configuration,
	bool needed,
	const std::string &where
)
{
	const auto found = configuration.find("endian");
	if (found == configuration.end() || found->is_null())
	{
		if (needed)
		{
			reject_metadata(
				where, "The bytes codec states no order of the bytes.");
		}
		return get_system_byte_order();
	}

	if (*found == "little")
	{
		return byte_order::little_endian;
	}
	if (*found == "big")
	{
		return byte_order::big_endian;
	}

	reject_metadata(where, "The bytes codec states an order of the bytes "
		"that is neither little nor big.");
}

bool is_lenient_extension(const nlohmann::json &value)
{
	if (!value.is_object())
	{
		return false;
	}

	const auto found = value.find("must_understand");
	return found != value.end() && *found == false;
}


// What the codecs of version 3 say of a chunk: where each axis lies in it,
// the order of its bytes and the codecs of its bytes, or the configuration
// of the sharding codec that stores it instead.
struct codec_chain
{
	std::vector<std::size_t> axes;
	byte_order order = get_system_byte_order();
	codec_list codecs;
	const nlohmann::json *sharding = nullptr;
};

// The codecs come in three runs: those of arrays into arrays, the one of the
// array into bytes, and those of bytes into bytes. The chunks of a shard
// may not be sharded again, and the codecs of an array that is sharded hold
// the sharding codec alone.
codec_chain parse_codec_chain(
	const nlohmann::json &listed,
	const nlohmann::json &data_type,
	std::size_t rank,
	bool inner,
	const std::string &where
)
{
	if (!listed.is_array() || listed.empty())
	{
		reject_metadata(where, "The codecs are no list, or none.");
	}

	codec_chain chain;
	chain.axes.resize(rank);
	std::iota(chain.axes.begin(), chain.axes.end(), std::size_t(0));
	bool transposed = false;
	bool serialized = false;
	for (const auto &codec : listed)
	{
		const auto &name =
			get_codec_name(codec, where).get_ref<const std::string&>();
		const auto &configuration = get_configuration(codec);

		if (name == "transpose")
		{
			if (serialized)
			{
				reject_metadata(where, "A transpose follows the bytes codec.");
			}
			apply_transpose(configuration, chain.axes, where);
			transposed = true;
		}
		else if (name == "bytes" || name == "sharding_indexed")
		{
			if (serialized)
			{
				reject_metadata(where, "The array is turned into bytes twice.");
			}
			serialized = true;

			if (name == "sharding_indexed")
			{
				if (inner)
				{
					refuse_metadata(
						where, "Shards of shards are not read.");
				}
				if (transposed)
				{
					refuse_metadata(
						where, "Shards transposed whole are not read.");
				}
				chain.sharding = &configuration;
				continue;
			}

			// Whether the order is needed depends on the size of the type,
			// which the order is read along with.
			const auto single = parse_v3_data_type(
				data_type, get_system_byte_order(), where).type;
			chain.order = parse_endian(
				configuration, get_size(single) > 1, where);
		}
		else if (const auto bytes_codec = make_v3_bytes_codec(name))
		{
			if (!serialized)
			{
				reject_metadata(
					where, "A codec of bytes comes before the bytes codec.");
			}
			if (chain.sharding != nullptr)
			{
				refuse_metadata(
					where, "Shards encoded whole are not read.");
			}
			chain.codecs.push_back(bytes_codec);
		}
		else
		{
			refuse_metadata(
				where, "Chunks encoded with \"" + name + "\" are not read.");
		}
	}
	if (!serialized)
	{
		reject_metadata(where, "No codec turns the array into bytes.");
	}

	return chain;
}

// The codecs of the index of a shard: its integers in some byte order, and
// maybe a checksum of them.
struct index_codecs
{
	byte_order order;
	bool checksum;
};

index_codecs parse_index_codecs(
	const nlohmann::json &listed,
	const std::string &where
)
{
	if (!listed.is_array() || listed.empty() || listed.size() > 2)
	{
		refuse_metadata(
			where, "Only an index of bytes, maybe checked, is read.");
	}

	if (get_codec_name(listed[0], where) != "bytes")
	{
		refuse_metadata(where, "Only an index of bytes is read.");
	}
	const auto order = parse_endian(get_configuration(listed[0]), true, where);

	if (listed.size() == 2 && get_codec_name(listed[1], where) != "crc32c")
	{
		refuse_metadata(
			where, "Only an index checked with crc32c, if at all, is read.");
	}

	return {order, listed.size() == 2};
}

zarr_index_location parse_index_location(
	const nlohmann::json &configuration,
	const std::string &where
)
{
	const auto found = configuration.find("index_location");
	if (found == configuration.end() || *found == "end")
	{
		return zarr_index_location::end;
	}
	if (*found == "start")
	{
		return zarr_index_location::start;
	}

	reject_metadata(where, "The index of a shard is neither at its start "
		"nor at its end.");
}

} // anonymous namespace

zarr_array_layout::zarr_array_layout(const zarr_node &node)
	: m_keys(std::string(), '.')
{
	if (node.kind != zarr_node_kind::array)
	{
		throw std::invalid_argument(
			node.metadata_path + ": zarr_array_layout: The node is not an "
			"array."
		);
	}

	const auto where = node.metadata_path + ": zarr_array_layout";
	try
	{
		if (node.version == 2)
		{
			read_v2(node, where);
		}
		else
		{
			read_v3(node, where);
		}
	}
	catch (const std::overflow_error &)
	{
		reject_metadata(where, "The chunks are too large to be counted.");
	}
}

void zarr_array_layout::read_v2(
	const zarr_node &node,
	const std::string &where
)
{
	const auto &metadata = node.metadata;

	m_shape = get_extents(get_member(metadata, "shape", where), "shape", where);
	auto chunks = get_chunk_extents(
		get_member(metadata, "chunks", where), m_shape.size(), where);
	const auto type = parse_v2_data_type(
		get_member(metadata, "dtype", where), where);

	std::vector<std::size_t> axes(m_shape.size());
	std::iota(axes.begin(), axes.end(), std::size_t(0));
	const auto &order = get_member(metadata, "order", where);
	if (order == "F")
	{
		std::reverse(axes.begin(), axes.end());
	}
	else if (order != "C")
	{
		reject_metadata(where, "The order is neither C nor F.");
	}

	const auto filters = metadata.find("filters");
	if (
		filters != metadata.end() && !filters->is_null() &&
		!(filters->is_array() && filters->empty())
	)
	{
		refuse_metadata(where, "Arrays with filters are not read.");
	}

	codec_list codecs;
	const auto &compressor = get_member(metadata, "compressor", where);
	if (!compressor.is_null())
	{
		codecs.push_back(make_v2_codec(compressor, where));
	}

	m_chunk.emplace(std::move(chunks), make_span(axes), type.type,
		type.order);
	m_fill = parse_fill_value(
		get_member(metadata, "fill_value", where), type.type, where);
	m_codecs = zarr_codec_pipeline(std::move(codecs));
	m_keys = parse_v2_chunk_key_encoding(metadata, where);
}

void zarr_array_layout::read_v3(
	const zarr_node &node,
	const std::string &where
)
{
	const auto &metadata = node.metadata;
	for (const auto &member : metadata.items())
	{
		const auto known = std::find_if(
			std::begin(v3_members), std::end(v3_members),
			[&member] (const char *name) { return member.key() == name; });
		if (known == std::end(v3_members) &&
			!is_lenient_extension(member.value()))
		{
			refuse_metadata(
				where,
				"The metadata states \"" + member.key() +
					"\", which this reader does not understand."
			);
		}
	}

	const auto transformers = metadata.find("storage_transformers");
	if (
		transformers != metadata.end() &&
		!(transformers->is_array() && transformers->empty())
	)
	{
		refuse_metadata(
			where, "Arrays with storage transformers are not read.");
	}

	m_shape = get_extents(get_member(metadata, "shape", where), "shape", where);

	const auto &grid = get_member(metadata, "chunk_grid", where);
	if (get_member(grid, "name", where) != "regular")
	{
		refuse_metadata(where, "Only a regular grid of chunks is read.");
	}
	auto chunks = get_chunk_extents(
		get_member(get_member(grid, "configuration", where), "chunk_shape",
			where),
		m_shape.size(),
		where
	);

	const auto &data_type = get_member(metadata, "data_type", where);
	const auto chain = parse_codec_chain(
		get_member(metadata, "codecs", where), data_type, m_shape.size(),
		false, where);

	if (chain.sharding == nullptr)
	{
		const auto type = parse_v3_data_type(data_type, chain.order, where);
		m_chunk.emplace(std::move(chunks), make_span(chain.axes), type.type,
			type.order);
		m_codecs = zarr_codec_pipeline(chain.codecs);
	}
	else
	{
		// The grid of the array is that of its shards, and what is decoded
		// is a chunk of a shard.
		const auto &sharding = *chain.sharding;
		auto inner = get_chunk_extents(
			get_member(sharding, "chunk_shape", where), m_shape.size(), where);
		const auto inner_chain = parse_codec_chain(
			get_member(sharding, "codecs", where), data_type, m_shape.size(),
			true, where);
		const auto index = parse_index_codecs(
			get_member(sharding, "index_codecs", where), where);

		try
		{
			m_shard.emplace(
				std::move(chunks), make_span(inner),
				parse_index_location(sharding, where), index.order,
				index.checksum);
		}
		catch (const std::invalid_argument &)
		{
			reject_metadata(where, "The chunks do not divide a shard.");
		}

		const auto type =
			parse_v3_data_type(data_type, inner_chain.order, where);
		m_chunk.emplace(std::move(inner), make_span(inner_chain.axes),
			type.type, type.order);
		m_codecs = zarr_codec_pipeline(inner_chain.codecs);
	}

	m_fill = parse_fill_value(
		get_member(metadata, "fill_value", where),
		m_chunk->get_data_type(),
		where
	);
	m_keys = parse_v3_chunk_key_encoding(
		get_member(metadata, "chunk_key_encoding", where), where);

	const auto names = metadata.find("dimension_names");
	if (names != metadata.end() && !names->is_null())
	{
		if (!names->is_array() || names->size() != m_shape.size())
		{
			reject_metadata(
				where, "The dimension names do not name every axis.");
		}
		for (const auto &name : *names)
		{
			if (name.is_string())
			{
				m_dimension_names.push_back(name.get<std::string>());
			}
			else if (name.is_null())
			{
				m_dimension_names.emplace_back();
			}
			else
			{
				reject_metadata(where, "A dimension name is no string.");
			}
		}
	}
}

span<const std::size_t> zarr_array_layout::get_shape() const noexcept
{
	return make_span(m_shape);
}

const zarr_chunk_layout& zarr_array_layout::get_chunk_layout() const noexcept
{
	return *m_chunk;
}

const zarr_shard_layout* zarr_array_layout::get_shard_layout() const noexcept
{
	return m_shard ? &*m_shard : nullptr;
}

span<const byte> zarr_array_layout::get_fill_element() const noexcept
{
	return make_span(m_fill);
}

const zarr_codec_pipeline& zarr_array_layout::get_codecs() const noexcept
{
	return m_codecs;
}

const zarr_chunk_key_encoding&
zarr_array_layout::get_key_encoding() const noexcept
{
	return m_keys;
}

const std::vector<std::string>&
zarr_array_layout::get_dimension_names() const noexcept
{
	return m_dimension_names;
}

} // namespace zarr
} // namespace vitrio
