<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# vitrio

vitrio is a high-throughput image I/O library tailored to the access
patterns and file formats common in Cryo-Electron Microscopy (CryoEM). This
is the reference of its C++ library. The Python package made on it has its
own, at [python/](https://gigabit-clowns.github.io/vitrio/python/), and the
[README](https://github.com/gigabit-clowns/vitrio#readme) and the
[examples](https://github.com/gigabit-clowns/vitrio/tree/main/examples)
show how the two are used.

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

```cpp
#include <vitrio/array/array.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_write.hpp>

int main()
{
	vitrio::file_image_reader_provider readers(
		vitrio::image_file_read_format_selector::get_shared()
	);

	// Read as 32-bit floats, whatever the file holds.
	vitrio::array image = vitrio::read(
		"micrograph.mrc", readers, vitrio::numerical_type::float32
	);

	vitrio::write_single(
		image, "copy.tif",
		*vitrio::image_file_write_format_selector::get_shared()
	);
}
```

The headers under `include/vitrio/` are the whole API, and each one
documents what it declares. Among them is how to read the events of an EER
file through vitrio::detector_event_image_reader_provider, and how to
register a format of your own with
vitrio::image_file_read_format_selector::register_format.
