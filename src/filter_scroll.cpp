// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>

#include "gen.h"
#include "filter_add_text.h"
#include "filter_scroll.h"
#include "error.h"
#include "utils.h"
#include "draw_text_bitmap.h"
#include "exec.h"

void blit(uint8_t *const out, const int w, const int h, const int x, const int y, const uint8_t *const in, const int in_w, const int in_h, const int off_x, const int off_y)
{
	for(int wy=off_y; wy<in_h; wy++) {
		int target_y = y + wy;
		if (target_y >= h)
			break;
		if (target_y < 0)
			continue;

		int out_offset = (y + wy) * w * 3;

		for(int wx=off_x; wx<in_w; wx++) {
			int target_x = wx + x;
			if (target_x >= w || target_x < 0)
				continue;

			int temp_offset = out_offset + target_x * 3;
			int in_offset = wy * in_w * 3 + wx * 3;

			out[temp_offset + 0] = in[in_offset + 0];
			out[temp_offset + 1] = in[in_offset + 1];
			out[temp_offset + 2] = in[in_offset + 2];
		}
	}
}

filter_scroll::filter_scroll(const std::string & font_file, const int x, const int y, const int text_w, const int n_lines, const int font_size, const std::string & exec_what, const bool horizontal_scroll, const std::optional<rgb_t> bg, const int scroll_speed, const rgb_t col, const bool invert, const std::map<std::string, feed *> & text_feeds) : font_file(font_file), x(x), y(y), text_w(text_w), n_lines(n_lines), font_size(font_size), exec_what(exec_what), horizontal_scroll(horizontal_scroll), bg(bg), scroll_speed(scroll_speed), col(col), invert(invert), text_feeds(text_feeds)
{
	restart_process();

	start();
}

filter_scroll::~filter_scroll()
{
	local_stop_flag = true;

	if (fd != -1)
		close(fd);

	stop();

	while(!buffer.empty()) {
		delete [] buffer.at(0).bitmap;

		buffer.erase(buffer.begin());
	}
}

void filter_scroll::operator()()
{
	while(!local_stop_flag) {
		auto [ has_data, data ] = poll_for_data();

		if (has_data && !data.empty()) {
			scroll_entry_t se { "", nullptr, 0, 0 };
			se.text = data;

			const std::lock_guard<std::mutex> lock(buffer_lock);

			while(buffer.size() >= n_lines) {
				delete [] buffer.at(0).bitmap;

				buffer.erase(buffer.begin());
			}

			buffer.push_back(se);
		}

		st->track_cpu_usage();
	}
}

void filter_scroll::restart_process()
{
	if (fd != -1)
		close(fd);

	pid_t pid;
	exec_with_pty(exec_what, &fd, &pid);
}

std::tuple<bool, std::string> filter_scroll::poll_for_data()
{
	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	if (poll(fds, 1, 1) == 1) {
		char in[4096];
		int rc = read(fd, in, sizeof in - 1);

		if (rc <= 0) {
			restart_process();

			return { false, "" };
		}

		in[rc] = 0x00;

		char *p = in;
		for(;;) {
			char *cr = strchr(p, '\r');
			if (!cr)
				break;

			*cr = ' ';
			p = cr;
		}

		in_buffer += in;
	}

	size_t lf = in_buffer.find('\n');
	if (lf != std::string::npos) {
		std::string out = in_buffer.substr(0, lf);
		in_buffer = in_buffer.substr(lf + 1);

		return { true, out };
	}

	return { false, "" };
}

void filter_scroll::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	int work_x = x < 0 ? x + w : x;
	int work_y = y < 0 ? y + h : y;

	if (horizontal_scroll) {
		std::string scroll_what;

		const std::lock_guard<std::mutex> lock(buffer_lock);
		for(auto & what : buffer) {
			if (what.bitmap == nullptr) {
				draw_text_bitmap dtm(font_file, unescape(what.text, ts, i, specific_int, text_feeds), font_size, true, bg, col, invert);

				auto dimensions = dtm.text_final_dimensions();
				what.w = std::get<0>(dimensions);
				what.h = std::get<1>(dimensions);

				size_t n = IMS(what.w, what.h, 3);

				uint8_t *bitmap = new uint8_t[n];
				memcpy(bitmap, dtm.get_bitmap(), n);
				what.bitmap = bitmap;
			}
		}

		int x = work_x, use_w = text_w == -1 ? w : text_w;
		size_t bitmap_nr = 0;
		bool first = true;

		while(x < use_w && !buffer.empty()) {
			int offset_x = first ? cur_x_pos : 0;
			first = false;

			blit(in_out, w, h, x - offset_x, work_y, buffer.at(bitmap_nr).bitmap, buffer.at(bitmap_nr).w, buffer.at(bitmap_nr).h, offset_x, 0);

			x += buffer.at(bitmap_nr).w - offset_x;

			bitmap_nr++;
			if (bitmap_nr >= buffer.size())
				bitmap_nr = 0;
		}

		if (!buffer.empty())
			cur_x_pos = (ts / (1000000 / scroll_speed)) % buffer.at(0).w;
	}
	else {
		const std::lock_guard<std::mutex> lock(buffer_lock);

		for(auto what : buffer) {
			std::string text_out = unescape(what.text, ts, i, specific_int, text_feeds);

			std::vector<std::string> *parts { nullptr };
			if (text_out.find("\n") != std::string::npos)
				parts = split(text_out.c_str(), "\n");
			else
				parts = split(text_out.c_str(), "\\n");

			for(std::string cl : *parts) {
				draw_text dt(font_file, cl, font_size, true, in_out, w, h, work_x, work_y, text_w == -1 ? w : text_w, bg, col, invert);

				work_y += font_size + 1;
			}

			delete parts;
		}
	}
}
