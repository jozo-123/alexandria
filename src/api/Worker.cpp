
#include "Worker.h"
#include "fcgio.h"
#include "config.h"
#include "parser/URL.h"
#include "parser/cc_parser.h"

#include "post_processor/PostProcessor.h"
#include "api/ApiResponse.h"
#include "hash_table/HashTable.h"
#include "full_text/FullText.h"
#include "full_text/FullTextIndex.h"
#include "full_text/FullTextRecord.h"
#include "full_text/SearchMetric.h"
#include "search_engine/SearchAllocation.h"
#include "Api.h"
#include "ApiStatusResponse.h"
#include "link/FullTextRecord.h"
#include "system/Logger.h"
#include "urlstore/UrlStore.h"

using namespace std;

namespace Worker {

	void test_search(const string &query) {
		SearchAllocation::Allocation *allocation = SearchAllocation::create_allocation();

		HashTable hash_table("main_index");
		HashTable hash_table_link("link_index");
		HashTable hash_table_domain_link("domain_link_index");

		FullTextIndex<FullTextRecord> index("main_index");
		FullTextIndex<Link::FullTextRecord> link_index("link_index");
		FullTextIndex<DomainLink::FullTextRecord> domain_link_index("domain_link_index");

		stringstream response_stream;
		Api::search(query, hash_table, index, link_index, domain_link_index, allocation, response_stream);

		cout << response_stream.rdbuf() << endl;

		SearchAllocation::delete_allocation(allocation);
	}

	void output_response(FCGX_Request &request, stringstream &response) {

		FCGX_FPrintF(request.out, "Content-type: application/json\r\n\r\n");
		FCGX_FPrintF(request.out, "%s", response.str().c_str());

	}

	void output_binary_response(FCGX_Request &request, stringstream &response) {

		FCGX_FPrintF(request.out, "Content-type: application/octet-stream\r\n\r\n");
		string data_out = response.str();
		cout << "OUTPUTTING : " << data_out.size() << " bytes" << endl;
		FCGX_PutStr(data_out.c_str(), response.str().size(), request.out);

	}

	void *run_worker(void *data) {

		SearchAllocation::Allocation *allocation = SearchAllocation::create_allocation();

		Worker *worker = static_cast<Worker *>(data);

		FCGX_Request request;

		FCGX_InitRequest(&request, worker->socket_id, 0);

		HashTable hash_table("main_index");
		HashTable hash_table_link("link_index");
		HashTable hash_table_domain_link("domain_link_index");

		FullTextIndex<FullTextRecord> index("main_index");
		FullTextIndex<Link::FullTextRecord> link_index("link_index");
		FullTextIndex<DomainLink::FullTextRecord> domain_link_index("domain_link_index");

		LOG_INFO("Server has started...");

		while (true) {

			static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;

			pthread_mutex_lock(&accept_mutex);
			int accept_response = FCGX_Accept_r(&request);
			pthread_mutex_unlock(&accept_mutex);

			if (accept_response < 0) {
				break;
			}

			const char *uri_ptr = FCGX_GetParam("REQUEST_URI", request.envp);
			const char *req_ptr = FCGX_GetParam("REQUEST_METHOD", request.envp);
			if ((uri_ptr == nullptr) || (req_ptr == nullptr)) {
				FCGX_Finish_r(&request);
				continue;
			}
			string uri(uri_ptr);
			string request_method(req_ptr);

			cout << request_method << endl;

			LOG_INFO("Serving request: " + uri);

			URL url("http://alexandria.org" + uri);

			auto query = url.query();

			stringstream response_stream;

			bool deduplicate = true;
			if (query.find("d") != query.end()) {
				if (query["d"] == "a") {
					deduplicate = false;
				}
			}

			if (query.find("q") != query.end() && deduplicate) {
				if (Config::index_text) {
					Api::search(query["q"], hash_table, index, link_index, domain_link_index, allocation, response_stream);
					output_response(request, response_stream);
				} else {
					Api::search_remote(query["q"], hash_table, link_index, domain_link_index, allocation, response_stream);
					output_response(request, response_stream);
				}
			} else if (query.find("q") != query.end() && !deduplicate) {
				Api::search_all(query["q"], hash_table, index, link_index, domain_link_index, allocation, response_stream);
				output_response(request, response_stream);
			} else if (query.find("s") != query.end()) {
				Api::word_stats(query["s"], index, link_index, hash_table.size(), hash_table_link.size(), response_stream);
				output_response(request, response_stream);
			} else if (query.find("u") != query.end()) {
				Api::url(query["u"], hash_table, response_stream);
				output_response(request, response_stream);
			} else if (query.find("i") != query.end()) {
				Api::ids(query["i"], index, allocation, response_stream);
				output_binary_response(request, response_stream);
			}

			FCGX_Finish_r(&request);
		}

		SearchAllocation::delete_allocation(allocation);

		return NULL;
	}

	void start_server() {
		FCGX_Init();

		int socket_id = FCGX_OpenSocket("127.0.0.1:8000", 20);
		if (socket_id < 0) {
			LOG_INFO("Could not open socket, exiting");
			return;
		}

		vector<pthread_t> thread_ids(Config::worker_count);

		Worker *workers = new Worker[Config::worker_count];
		for (size_t i = 0; i < Config::worker_count; i++) {
			workers[i].socket_id = socket_id;
			workers[i].thread_id = i;

			pthread_create(&thread_ids[i], NULL, run_worker, &workers[i]);
		}

		for (size_t i = 0; i < Config::worker_count; i++) {
			pthread_join(thread_ids[i], NULL);
		}
	}

	void download_server() {
		Parser::warc_downloader();
	}

	void status_server(Status *status) {
		FCGX_Init();

		int socket_id = FCGX_OpenSocket("127.0.0.1:8000", 20);
		if (socket_id < 0) {
			LOG_INFO("Could not open socket, exiting");
			return;
		}

		FCGX_Request request;
		FCGX_InitRequest(&request, socket_id, 0);

		LOG_INFO("Status server has started...");

		while (true) {

			int accept_response = FCGX_Accept_r(&request);
			if (accept_response < 0) {
				break;
			}

			stringstream response_stream;

			ApiStatusResponse api_response(*status);

			response_stream << api_response;

			output_response(request, response_stream);

			FCGX_Finish_r(&request);
		}
	}

	void urlstore_server() {
		FCGX_Init();

		int socket_id = FCGX_OpenSocket("127.0.0.1:8001", 20);
		if (socket_id < 0) {
			LOG_INFO("Could not open socket, exiting");
			return;
		}

		FCGX_Request request;
		FCGX_InitRequest(&request, socket_id, 0);

		LOG_INFO("Urlstore server has started...");

		UrlStore::UrlStore url_store("/mnt/0/urlstore");

		const size_t max_post_len = 100*1024*1024;
		const size_t buffer_len = 1024*1024;
		char *buffer = new char[buffer_len];

		int error = 0;
		while (true) {

			error = 0;

			int accept_response = FCGX_Accept_r(&request);
			if (accept_response < 0) {
				break;
			}

			const char *uri_ptr = FCGX_GetParam("REQUEST_URI", request.envp);
			const char *req_ptr = FCGX_GetParam("REQUEST_METHOD", request.envp);

			if ((uri_ptr == nullptr) || (req_ptr == nullptr)) {
				FCGX_Finish_r(&request);
				continue;
			}

			string uri(uri_ptr);
			string request_method(req_ptr);

			stringstream response_stream;

			if (request_method == "PUT") {
				string post_data;
				while (true) {

					const size_t read_bytes = FCGX_GetStr(buffer, buffer_len, request.in);
					if (read_bytes == 0) break;

					if (post_data.size() + read_bytes > max_post_len) {
						error = 1;
						LOG_ERROR("Posted data larger then " + to_string(max_post_len) + ", ignoring request");
						break;
					}
					post_data.append(buffer, read_bytes);
				}

				if (error == 0) {
					UrlStore::handle_put_request(url_store, post_data, response_stream);
				}
			} else if (request_method == "GET") {

				//char **env = request.envp;
				//while (*(++env)) cout << *env << endl;

				const char *accept_ptr = FCGX_GetParam("HTTP_ACCEPT", request.envp);
				string accept = "text/html";
				if (accept_ptr != nullptr) {
					accept = string(accept_ptr);
				}
				
				const char *uri_ptr = FCGX_GetParam("REQUEST_URI", request.envp);
				if (uri_ptr != nullptr) {
					string uri(uri_ptr);
					uri.replace(0, 10, "");
					if (accept == "application/octet-stream") {
						UrlStore::handle_binary_get_request(url_store, URL(uri), response_stream);
						output_binary_response(request, response_stream);
					} else {
						UrlStore::handle_get_request(url_store, URL(uri), response_stream);
						output_response(request, response_stream);
					}
				}
			}


			FCGX_Finish_r(&request);
		}
	}

	thread download_server_thread;
	void start_download_server() {

		download_server_thread = std::move(thread(download_server));

	}

	thread status_server_thread;
	void start_status_server(Status &status) {

		status_server_thread = std::move(thread(status_server, &status));

	}

	thread urlstore_server_thread;
	void start_urlstore_server() {

		urlstore_server_thread = std::move(thread(urlstore_server));

	}
}
