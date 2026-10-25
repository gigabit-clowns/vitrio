// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_multiscale.hpp"

#include "zarr_json.hpp"

namespace vitrio
{
namespace zarr
{

namespace
{

const nlohmann::json* find_multiscales(const nlohmann::json &attributes)
{
	if (!attributes.is_object())
	{
		return nullptr;
	}

	const auto ome = attributes.find("ome");
	if (ome != attributes.end() && ome->is_object())
	{
		const auto found = ome->find("multiscales");
		if (found != ome->end())
		{
			return &*found;
		}
	}

	const auto found = attributes.find("multiscales");
	return found == attributes.end() ? nullptr : &*found;
}

std::string get_optional_string(
	const nlohmann::json &object,
	const char *key,
	const std::string &where
)
{
	const auto found = object.find(key);
	if (found == object.end() || found->is_null())
	{
		return std::string();
	}
	if (!found->is_string())
	{
		reject_metadata(
			where, std::string("The ") + key + " of an axis is no string.");
	}

	return found->get<std::string>();
}

zarr_axis parse_axis(const nlohmann::json &axis, const std::string &where)
{
	if (axis.is_string())
	{
		const auto name = axis.get<std::string>();
		std::string type;
		if (name == "x" || name == "y" || name == "z")
		{
			type = "space";
		}
		else if (name == "t")
		{
			type = "time";
		}
		else if (name == "c")
		{
			type = "channel";
		}
		return {name, type, std::string()};
	}

	if (!axis.is_object())
	{
		reject_metadata(where, "An axis is neither a name nor an object.");
	}

	const auto &name = get_member(axis, "name", where);
	if (!name.is_string())
	{
		reject_metadata(where, "The name of an axis is no string.");
	}

	return {
		name.get<std::string>(),
		get_optional_string(axis, "type", where),
		get_optional_string(axis, "unit", where)
	};
}

std::vector<double> parse_scale(
	const nlohmann::json &dataset,
	const std::string &where
)
{
	const auto transformations = dataset.find("coordinateTransformations");
	if (transformations == dataset.end() || !transformations->is_array())
	{
		return std::vector<double>();
	}

	for (const auto &transformation : *transformations)
	{
		if (!transformation.is_object() ||
			transformation.value("type", std::string()) != "scale")
		{
			continue;
		}

		const auto &scale = get_member(transformation, "scale", where);
		if (!scale.is_array())
		{
			reject_metadata(where, "A scale is no list.");
		}

		std::vector<double> values;
		for (const auto &value : scale)
		{
			if (!value.is_number())
			{
				reject_metadata(where, "A scale holds what is no number.");
			}
			values.push_back(value.get<double>());
		}
		return values;
	}

	return std::vector<double>();
}

} // anonymous namespace

boost::optional<zarr_multiscale> parse_multiscale(
	const nlohmann::json &attributes,
	const std::string &where
)
{
	const auto *const multiscales = find_multiscales(attributes);
	if (multiscales == nullptr)
	{
		return boost::none;
	}
	if (!multiscales->is_array() || multiscales->empty())
	{
		reject_metadata(where, "The multiscales are no list, or none.");
	}

	const auto &image = multiscales->front();
	zarr_multiscale result;

	const auto axes = image.is_object() ? image.find("axes") : image.end();
	if (image.is_object() && axes != image.end())
	{
		if (!axes->is_array())
		{
			reject_metadata(where, "The axes are no list.");
		}
		for (const auto &axis : *axes)
		{
			result.axes.push_back(parse_axis(axis, where));
		}
	}

	const auto &datasets = get_member(image, "datasets", where);
	if (!datasets.is_array() || datasets.empty())
	{
		reject_metadata(where, "The image has no levels.");
	}
	for (const auto &dataset : datasets)
	{
		const auto &path = get_member(dataset, "path", where);
		if (!path.is_string() || path.get_ref<const std::string&>().empty())
		{
			reject_metadata(where, "A level names no path.");
		}
		result.datasets.push_back(
			{path.get<std::string>(), parse_scale(dataset, where)});
	}

	return result;
}

} // namespace zarr
} // namespace vitrio
