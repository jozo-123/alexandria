/*
 * MIT License
 *
 * Alexandria.org
 *
 * Copyright (c) 2021 Josef Cullhed, <info@alexandria.org>, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <iostream>
#include "parser/URL.h"
#include "json.hpp"

namespace UrlStore {

	const size_t update_robots       = 0B00000001;

	class RobotsData {
		public:
			RobotsData();
			explicit RobotsData(const std::string &str);
			RobotsData(const char *cstr, size_t len);
			~RobotsData();

			std::string m_domain;
			std::string m_robots;

			void apply_update(const RobotsData &data, size_t update_bitmask);

			std::string to_str() const;
			std::string private_key() const;
			std::string public_key() const;
			nlohmann::ordered_json to_json() const;

			static std::string public_key_to_private_key(const std::string &public_key) {
				return public_key;
			}

			static const std::string uri;
	};

}
