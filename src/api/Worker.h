
#pragma once

#include <iostream>

namespace Worker {

	struct Status {

		size_t items;
		size_t items_indexed;
		size_t start_time;

	};

	struct Worker {

		int socket_id;
		int thread_id;

	};

	void start_server();
	void start_status_server(Status &status);

}
