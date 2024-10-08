#pragma once
#ifndef _CPP_TEXTURE_PACKER_UTILS_
#define _CPP_TEXTURE_PACKER_UTILS_

#include <vector>
#include <tuple>
#include <string>
#include <fstream>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <codecvt>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#define cimg_display 0
#define cimg_use_jpeg
#define cimg_use_png
#include <CImg.h>

#include <plist/plist.h>

#include "image_info.h"
#include "atlas.h"
#include "utf8_convert.h"

namespace CppTexturePacker
{
	namespace fs = boost::filesystem;

	using Image = cimg_library::CImg<unsigned char>;
	using ImageInfoMap = std::unordered_map<unsigned int, ImageInfo>;

	Image convert_to_rgba_image(const Image &src)
	{
		if (src.spectrum() == 3)
		{
			Image dst = Image(src.width(), src.height(), src.depth(), 4);

			for (int w = 0; w < src.width(); ++w)
			{
				for (int h = 0; h < src.height(); ++h)
				{
					for (int d = 0; d < src.depth(); ++d)
					{
						dst(w, h, d, 0) = src(w, h, d, 0);
						dst(w, h, d, 1) = src(w, h, d, 1);
						dst(w, h, d, 2) = src(w, h, d, 2);
						dst(w, h, d, 3) = 255;
					}
				}
			}

			return dst;
		}

		assert(src.spectrum() == 4); //unknown image channels
		return src;
	}

	Image read_image_from_file(const std::string &file_path)
	{
		return convert_to_rgba_image(Image(file_path.c_str()));
	}

	void save_image_to_file(const std::string &file_path, const Image &image)
	{
		if (file_path.size() < 4)
		{
			return;
		}

		auto suffix = file_path.substr(file_path.size() - 4);
		if (suffix.compare(".bmp") == 0)
		{
			image.save_bmp(file_path.c_str());
		}
		else if (suffix.compare(".jpg") == 0)
		{
			image.save_jpeg(file_path.c_str());
		}
		else if (suffix.compare(".png") == 0)
		{
			image.save_png(file_path.c_str());
		}
	}

	ImageInfo read_image_info_from_file(const std::string &file_path)
	{
		fs::path fp = file_path;
		auto abs_fp = fs::absolute(fp);
		return ImageInfo(read_image_from_file(abs_fp.string()), abs_fp.string());
	}

	std::vector<ImageInfo> load_image_infos_from_paths(const std::vector<std::string> &file_paths)
	{
		std::vector<ImageInfo> image_infos;

		for (auto file_path : file_paths)
		{
			image_infos.emplace_back(ImageInfo(read_image_from_file(file_path), file_path));
		}

		return image_infos;
	}

	std::vector<ImageInfo> load_image_infos_from_dir(const std::string &dir_path)
	{
		std::vector<std::string> file_paths;

		for (auto &fe : fs::recursive_directory_iterator(dir_path))
		{
			auto file_path = fe.path().string();
			auto suffix = file_path.substr(file_path.size() - 4);
			if (
				suffix.compare(".bmp") == 0 ||
				suffix.compare(".jpg") == 0 ||
				suffix.compare(".png") == 0)
			{
				file_paths.emplace_back(file_path);
			}
		}

		return load_image_infos_from_paths(file_paths);
	}

	bool _is_border_pixel(const Image &image, int x, int y)
	{
		int width = image.width();
		int height = image.height();

		if (image(x, y, 0, 3) == 0)
		{
			return false;
		}

		int offsets[3] = {-1, 0, 1};
		for (int offset_x : offsets)
		{
			int nx = x + offset_x;
			for (int offset_y : offsets)
			{
				int ny = y + offset_y;
				if (nx >= 0 && nx < width &&
					ny >= 0 && ny < height &&
					image(nx, ny, 0, 3) == 0)
				{
					return true;
				}
			}
		}

		return false;
	}

	void alpha_bleeding(Image &image, size_t bleeding_pixel = 4)
	{
		using Vec2 = std::tuple<int, int>;

		std::vector<Vec2> borders0;
		std::vector<Vec2> borders1;
		int width = image.width();
		int height = image.height();

		for (int x = 0; x < width; ++x)
		{
			for (int y = 0; y < height; ++y)
			{
				if (_is_border_pixel(image, x, y))
				{
					borders0.emplace_back(Vec2{x, y});
				}
			}
		}

		int offsets[3] = {-1, 0, 1};
		std::vector<Vec2> *borders = &borders0;
		std::vector<Vec2> *new_borders = &borders1;

		for (int i = 0; i < bleeding_pixel; ++i)
		{
			for (auto border : *borders)
			{
				int x = std::get<0>(border);
				int y = std::get<1>(border);

				for (int offset_x : offsets)
				{
					int nx = x + offset_x;
					for (int offset_y : offsets)
					{
						int ny = y + offset_y;
						if (nx >= 0 && nx < width &&
							ny >= 0 && ny < height &&
							image(nx, ny, 0, 3) == 0)
						{
							image(nx, ny, 0, 0) = image(x, y, 0, 0);
							image(nx, ny, 0, 1) = image(x, y, 0, 1);
							image(nx, ny, 0, 2) = image(x, y, 0, 2);
							image(nx, ny, 0, 3) = 1;

							if (_is_border_pixel(image, nx, ny))
							{
								new_borders->emplace_back(Vec2{nx, ny});
							}
						}
					}
				}
			}

			auto tmp = borders;
			borders = new_borders;
			new_borders = tmp;
			new_borders->clear();
		}
	}

	void alpha_remove(Image &image)
	{
		for (int w = 0; w < image.width(); ++w)
		{
			for (int h = 0; h < image.height(); ++h)
			{
				for (int d = 0; d < image.depth(); ++d)
				{
					image(w, h, d, 3) = 255;
				}
			}
		}
	}

	void clean_pixel_alpha_below(Image &image, unsigned char alpha)
	{
		for (int w = 0; w < image.width(); ++w)
		{
			for (int h = 0; h < image.height(); ++h)
			{
				for (int d = 0; d < image.depth(); ++d)
				{
					if (image(w, h, d, 3) < alpha)
					{
						image(w, h, d, 0) = 0;
						image(w, h, d, 1) = 0;
						image(w, h, d, 2) = 0;
						image(w, h, d, 3) = 0;
					}
				}
			}
		}
	}

	Rect<int> get_bounding_box(const Image &image)
	{
		int width = image.width(),
			height = image.height();

		int l = width,
			t = height,
			r = 0,
			b = 0;

		for (int x = 0; x < width; ++x)
		{
			for (int y = 0; y < height; ++y)
			{
				if (image(x, y, 0, 3) != 0)
				{
					if (x < l)
					{
						l = x;
					}
					if (y < t)
					{
						t = y;
					}
					if (x > r)
					{
						r = x;
					}

					if (y > b)
					{
						b = y;
					}
				}
			}
		}

		return Rect<int>{l, t, r - l + 1, b - t + 1};
	}

	void trim_image(Image &image, const Rect<int> &rect)
	{
		image.crop(rect.get_left(), rect.get_top(), rect.get_right(), rect.get_bottom(), 0);
	}

	void dump_altalas_to_plist(
		const std::string &file_path,
		const Atlas &atlas,
		ImageInfoMap &image_info_map,
		const std::string &texture_file_name,
		const std::string &base_image_path,
		bool use_backslash = false)
	{
		char *plist_xml = NULL;
		uint32_t size_out = 0;

		auto plist_root = plist_new_dict();
		auto frames_dict = plist_new_dict();
		auto metadata_dict = plist_new_dict();

		plist_dict_set_item(plist_root, "frames", frames_dict);
		plist_dict_set_item(plist_root, "metadata", metadata_dict);

		plist_dict_set_item(metadata_dict, "format", plist_new_uint(2));
		plist_dict_set_item(metadata_dict, "textureFileName", plist_new_string(texture_file_name.c_str()));
		plist_dict_set_item(metadata_dict, "realTextureFileName", plist_new_string(texture_file_name.c_str()));
		plist_dict_set_item(metadata_dict, "size", plist_new_string((boost::format("{%d,%d}") % atlas.get_width() % atlas.get_height()).str().c_str()));

		// std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> wconvert;

		for (const ImageRect &image_rect : atlas.get_placed_image_rect())
		{
			auto image_info = image_info_map[image_rect.ex_key];

			auto source_bbox = image_info.get_source_bbox();
			auto source_rect = image_info.get_source_rect();

			auto source_bbox_x = source_bbox.x - image_info.get_extruded() - image_info.get_inner_padding();
			auto source_bbox_y = source_bbox.y - image_info.get_extruded() - image_info.get_inner_padding();

			int width = image_rect.width, height = image_rect.height;
			if (image_rect.is_rotated())
			{
				width = image_rect.height;
				height = image_rect.width;
			}

			int center_offset_x = 0, center_offset_y = 0;
			if (image_info.is_trimmed())
			{

				center_offset_x = int(source_bbox_x + source_bbox.width / 2. - source_rect.width / 2.);
				center_offset_y = -int(source_bbox_y + source_bbox.height / 2. - source_rect.height / 2.);
			}

			fs::path image_path = image_info.get_image_path();
			if (base_image_path != "")
			{
				std::string image_path_str = image_info.get_image_path();
				std::string base_path_str = base_image_path;
				if (use_backslash)
				{
					std::replace(base_path_str.begin(), base_path_str.end(), '/', '\\');
					std::replace(image_path_str.begin(), image_path_str.end(), '/', '\\');
				}
				else
				{
					std::replace(base_path_str.begin(), base_path_str.end(), '\\', '/');
					std::replace(image_path_str.begin(), image_path_str.end(), '\\', '/');
				}

				image_path_str = fs::absolute(fs::path(image_path_str)).string();
				base_path_str = fs::absolute(fs::path(base_path_str)).string();
				
				if (base_path_str.size() < image_path_str.size() && base_path_str == image_path_str.substr(0, base_path_str.size()))
				{
					image_path = fs::path(image_path_str.substr(base_path_str.size()));
				}
			}
			else
			{
				image_path = image_path.filename();
			}

			auto frame_data = plist_new_dict();
			plist_dict_set_item(frame_data, "frame", plist_new_string((boost::format("{{%d,%d},{%d,%d}}") % (image_rect.x + image_info.get_extruded() + image_info.get_inner_padding()) % (image_rect.y + image_info.get_extruded() + image_info.get_inner_padding()) % source_bbox.width % source_bbox.height).str().c_str()));
			plist_dict_set_item(frame_data, "offset", plist_new_string((boost::format("{%d,%d}") % center_offset_x % center_offset_y).str().c_str()));
			plist_dict_set_item(frame_data, "rotated", plist_new_bool(image_rect.is_rotated()));
			plist_dict_set_item(frame_data, "sourceColorRect", plist_new_string((boost::format("{{%d,%d},{%d,%d}}") % source_bbox_x % source_bbox_y % source_bbox.width % source_bbox.height).str().c_str()));
			plist_dict_set_item(frame_data, "sourceSize", plist_new_string((boost::format("{%d,%d}") % source_rect.width % source_rect.height).str().c_str()));

			auto image_path_wstring = image_path.wstring();
			plist_dict_set_item(frames_dict, utf8_encode(image_path_wstring).c_str(), frame_data);
		}

		plist_to_xml(plist_root, &plist_xml, &size_out);
		plist_free(plist_root);

		if (size_out)
		{
			fs::path fp = file_path;
			auto abs_fp = fs::absolute(fp);
			std::fstream fs;
			fs.open(abs_fp.string(), std::fstream::out);
			fs << plist_xml;
			fs.close();
		}

		free(plist_xml);
	}

	void draw_image_in_image(Image &main_image, const Image &sub_image, int start_x, int start_y, bool rotated = false)
	{
		int m_width = main_image.width(),
			m_height = main_image.height(),
			s_width = sub_image.width(),
			s_height = sub_image.height();

		if (rotated)
		{
			// clockwise 90
			for (int offset_x = 0; offset_x < s_width && start_y + offset_x < m_height; ++offset_x)
			{
				int my = start_y + offset_x;
				for (int offset_y = 0; offset_y < s_height; ++offset_y)
				{
					int mx = start_x + s_height - 1 - offset_y;
					if (mx >= m_width)
					{
						continue;
					}
					main_image(mx, my, 0, 0) = sub_image(offset_x, offset_y, 0, 0);
					main_image(mx, my, 0, 1) = sub_image(offset_x, offset_y, 0, 1);
					main_image(mx, my, 0, 2) = sub_image(offset_x, offset_y, 0, 2);
					main_image(mx, my, 0, 3) = sub_image(offset_x, offset_y, 0, 3);
				}
			}
		}
		else
		{
			for (int offset_x = 0; offset_x < s_width && start_x + offset_x < m_width; ++offset_x)
			{
				for (int offset_y = 0; offset_y < s_height && start_y + offset_y < m_height; ++offset_y)
				{
					main_image(start_x + offset_x, start_y + offset_y, 0, 0) = sub_image(offset_x, offset_y, 0, 0);
					main_image(start_x + offset_x, start_y + offset_y, 0, 1) = sub_image(offset_x, offset_y, 0, 1);
					main_image(start_x + offset_x, start_y + offset_y, 0, 2) = sub_image(offset_x, offset_y, 0, 2);
					main_image(start_x + offset_x, start_y + offset_y, 0, 3) = sub_image(offset_x, offset_y, 0, 3);
				}
			}
		}
	}

	Image dump_altalas_to_image(const Atlas &atlas, ImageInfoMap &image_info_map)
	{
		Image image(atlas.get_width(), atlas.get_height(), 1, 4, 0);
		for (auto image_rect : atlas.get_placed_image_rect())
		{
			auto image_info = image_info_map[image_rect.ex_key];
			draw_image_in_image(image, image_info.get_image(), image_rect.x, image_rect.y, image_rect.is_rotated());
		}
		return image;
	}

	ImageInfoMap make_image_info_map(const std::vector<ImageInfo> &image_infos)
	{
		ImageInfoMap image_info_map;
		for (const ImageInfo &image_info : image_infos)
		{
			image_info_map.emplace(image_info.get_ex_key(), image_info);
		}
		return image_info_map;
	}

	Image enlarge_image_border(const Image &image, unsigned char pixel_num, bool repeat_border = false)
	{
		Image new_image(image.width() + pixel_num * 2, image.height() + pixel_num * 2, 1, 4, 0);
		draw_image_in_image(new_image, image, pixel_num, pixel_num);

		if (repeat_border)
		{
			int width = new_image.width();
			int height = new_image.height();

			for (int offset_y = 0; offset_y < height; ++offset_y)
			{
				for (int offset_x = 0; offset_x < pixel_num; ++offset_x)
				{
					new_image(offset_x, offset_y, 0, 0) = new_image(pixel_num, offset_y, 0, 0);
					new_image(offset_x, offset_y, 0, 1) = new_image(pixel_num, offset_y, 0, 1);
					new_image(offset_x, offset_y, 0, 2) = new_image(pixel_num, offset_y, 0, 2);
					new_image(offset_x, offset_y, 0, 3) = new_image(pixel_num, offset_y, 0, 3);

					new_image(width - pixel_num + offset_x, offset_y, 0, 0) = new_image(width - pixel_num - 1, offset_y, 0, 0);
					new_image(width - pixel_num + offset_x, offset_y, 0, 1) = new_image(width - pixel_num - 1, offset_y, 0, 1);
					new_image(width - pixel_num + offset_x, offset_y, 0, 2) = new_image(width - pixel_num - 1, offset_y, 0, 2);
					new_image(width - pixel_num + offset_x, offset_y, 0, 3) = new_image(width - pixel_num - 1, offset_y, 0, 3);
				}
			}

			for (int offset_x = 0; offset_x < width; ++offset_x)
			{
				for (int offset_y = 0; offset_y < pixel_num; ++offset_y)
				{
					new_image(offset_x, offset_y, 0, 0) = new_image(offset_x, pixel_num, 0, 0);
					new_image(offset_x, offset_y, 0, 1) = new_image(offset_x, pixel_num, 0, 1);
					new_image(offset_x, offset_y, 0, 2) = new_image(offset_x, pixel_num, 0, 2);
					new_image(offset_x, offset_y, 0, 3) = new_image(offset_x, pixel_num, 0, 3);

					new_image(offset_x, height - pixel_num + offset_y, 0, 0) = new_image(offset_x, height - pixel_num - 1, 0, 0);
					new_image(offset_x, height - pixel_num + offset_y, 0, 1) = new_image(offset_x, height - pixel_num - 1, 0, 1);
					new_image(offset_x, height - pixel_num + offset_y, 0, 2) = new_image(offset_x, height - pixel_num - 1, 0, 2);
					new_image(offset_x, height - pixel_num + offset_y, 0, 3) = new_image(offset_x, height - pixel_num - 1, 0, 3);
				}
			}
		}

		return new_image;
	}

} // namespace CppTexturePacker

#endif
