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

#include "KeyValueStore.h"

using namespace std;

KeyValueStore::KeyValueStore(const string &db_name) {
	leveldb::Options options;
	options.create_if_missing = true;
	options.write_buffer_size = 1024ull * 1024ull * 1024ull;
	options.max_open_files = 100000;
	leveldb::Status status = leveldb::DB::Open(options, db_name, &m_db);
	if (!status.ok()) {
		cout << "Could not open database: " << db_name << endl;
	}
}

KeyValueStore::~KeyValueStore() {
	delete m_db;
}

string KeyValueStore::get(const string &key_str) const {
	leveldb::Slice key = key_str;
	string value;
	leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
	return value;
}

void KeyValueStore::set(const string &key_str, const string &value) {
	leveldb::Slice key = key_str;
	leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
	if (!s.ok()) {
		cerr << s.ToString() << endl;
	}
}

bool KeyValueStore::is_full() {
	string num_files;
	m_db->GetProperty("leveldb.num-files-at-level0", &num_files);
	size_t num = stoull(num_files);

	cout << "level0: " << num << " - ";
	cout << "leveldb.num-files-at-level: ";
	for (size_t level = 0; level < 10; level++) {
		string num_files;
		m_db->GetProperty("leveldb.num-files-at-level" + to_string(level), &num_files);
		cout << num_files << " ";
	}
	cout << endl;

	return num >= 10;
}

void KeyValueStore::compact() {
	m_db->CompactRange(nullptr, nullptr);
}

