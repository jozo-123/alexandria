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

#include <iostream>
#include "fcgio.h"
#include "config.h"
#include "parser/URL.h"

#include "post_processor/PostProcessor.h"
#include "api/ApiResponse.h"

#include "hash_table/HashTable.h"

#include "full_text/FullText.h"
#include "full_text/FullTextIndex.h"
#include "full_text/FullTextRecord.h"
#include "full_text/SearchMetric.h"

#include "api/Api.h"

#include "link_index/LinkFullTextRecord.h"

#include "system/Logger.h"

using namespace std;

struct Worker {

	int socket_id;
	int thread_id;

};

void output_response(FCGX_Request &request, stringstream &response) {

	FCGX_FPrintF(request.out, "Content-type: application/json\r\n\r\n");
	FCGX_FPrintF(request.out, "%s", response.str().c_str());

}

static void *run_worker(void *data) {
	Worker *worker = (Worker *)data;
	int rc, i, thread_id = worker->thread_id;
	pid_t pid = getpid();

	FCGX_Request request;

	FCGX_InitRequest(&request, worker->socket_id, 0);

	HashTable hash_table("main_index");
	HashTable hash_table_link("link_index");
	HashTable hash_table_domain_link("domain_link_index");

	vector<FullTextIndex<FullTextRecord> *> index_array = FullText::create_index_array<FullTextRecord>("main_index", Config::ft_num_partitions);
	vector<FullTextIndex<LinkFullTextRecord> *> link_index_array =
		FullText::create_index_array<LinkFullTextRecord>("link_index", Config::ft_num_link_partitions);
	vector<FullTextIndex<DomainLinkFullTextRecord> *> domain_link_index_array =
		FullText::create_index_array<DomainLinkFullTextRecord>("domain_link_index", Config::ft_num_link_partitions);

	LogInfo("Server has started...");

	while (true) {

		static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;

		pthread_mutex_lock(&accept_mutex);
		int accept_response = FCGX_Accept_r(&request);
		pthread_mutex_unlock(&accept_mutex);

		if (accept_response < 0) {
			break;
		}

		string uri = FCGX_GetParam("REQUEST_URI", request.envp);

		LogInfo("Serving request: " + uri);

		URL url("http://alexandria.org" + uri);

		auto query = url.query();

		stringstream response_stream;

		bool deduplicate = true;
		if (query.find("d") != query.end()) {
			if (query["d"] == "a") {
				deduplicate = false;
			}
		}

		Profiler::tick("request", "initialize_request");
		if (query.find("q") != query.end() && deduplicate) {
			Api::search(query["q"], hash_table, index_array, link_index_array, domain_link_index_array, response_stream);
		} else if (query.find("q") != query.end() && !deduplicate) {
			Api::search_all(query["q"], hash_table, index_array, link_index_array, domain_link_index_array, response_stream);
		} else if (query.find("s") != query.end()) {
			Api::word_stats(query["s"], index_array, link_index_array, hash_table.size(), hash_table_link.size(), response_stream);
		} else if (query.find("u") != query.end()) {
			Api::url(query["u"], hash_table, response_stream);
		} else if (query.find("l") != query.end()) {
			Api::search_links(query["l"], hash_table_link, link_index_array, response_stream);
		} else if (query.find("d") != query.end()) {
			Api::search_domain_links(query["d"], hash_table_domain_link, domain_link_index_array, response_stream);
		}
		Profile::end("request");

		output_response(request, response_stream);

		FCGX_Finish_r(&request);
	}

	FullText::delete_index_array<FullTextRecord>(index_array);
	FullText::delete_index_array<LinkFullTextRecord>(link_index_array);
	FullText::delete_index_array<DomainLinkFullTextRecord>(domain_link_index_array);

	return NULL;
}

int main(void) {

	/*
	stringstream response_stream;
	HashTable hash_table("main_index");
	HashTable hash_table_link("link_index");
	HashTable hash_table_domain_link("domain_link_index");

	vector<FullTextIndex<FullTextRecord> *> index_array = FullText::create_index_array<FullTextRecord>("main_index", Config::ft_num_partitions);
	vector<FullTextIndex<LinkFullTextRecord> *> link_index_array =
		FullText::create_index_array<LinkFullTextRecord>("link_index", Config::ft_num_link_partitions);
	vector<FullTextIndex<DomainLinkFullTextRecord> *> domain_link_index_array =
		FullText::create_index_array<DomainLinkFullTextRecord>("domain_link_index", Config::ft_num_link_partitions);

	Api::search("cullhed", hash_table, index_array, link_index_array, domain_link_index_array, response_stream);

	FullText::delete_index_array<FullTextRecord>(index_array);
	FullText::delete_index_array<LinkFullTextRecord>(link_index_array);
	FullText::delete_index_array<DomainLinkFullTextRecord>(domain_link_index_array);

	return 0;*/

	FCGX_Init();

	int socket_id = FCGX_OpenSocket("127.0.0.1:8000", 20);
	if (socket_id < 0) {
		LogInfo("Could not open socket, exiting");
		return 1;
	}

	pthread_t thread_ids[Config::worker_count];

	Worker *workers = new Worker[Config::worker_count];
	for (int i = 0; i < Config::worker_count; i++) {
		workers[i].socket_id = socket_id;
		workers[i].thread_id = i;

		pthread_create(&thread_ids[i], NULL, run_worker, &workers[i]);
	}

	for (int i = 0; i < Config::worker_count; i++) {
		pthread_join(thread_ids[i], NULL);
	}
	
	return 0;
}

