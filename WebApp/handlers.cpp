#include "handlers.h"

#include <vector>

#include "rendering.h"

// register an orm mapping (to convert the db query results into
// json objects).
// the db query results contain several rows, each has a number of
// fields. the order of `make_db_field<Type[i]>(name[i])` in the
// initializer list corresponds to these fields (`Type[0]` and
// `name[0]` correspond to field[0], `Type[1]` and `name[1]`
// correspond to field[1], ...). `Type[i]` is the type you want
// to convert the field value to, and `name[i]` is the identifier
// with which you want to store the field in the json object, so
// if the returned json object is `obj`, `obj[name[i]]` will have
// the type of `Type[i]` and store the value of field[i].
bserv::db_relation_to_object orm_user{
	bserv::make_db_field<int>("id"),
	bserv::make_db_field<std::string>("username"),
	bserv::make_db_field<std::string>("password"),
	bserv::make_db_field<bool>("is_superuser"),
	bserv::make_db_field<std::string>("first_name"),
	bserv::make_db_field<std::string>("last_name"),
	bserv::make_db_field<std::string>("phone_number"),
	bserv::make_db_field<bool>("is_active")
};

bserv::db_relation_to_object orm_flight{
	bserv::make_db_field<int>("id"),
	bserv::make_db_field<std::string>("flight_number"),
	bserv::make_db_field<std::string>("departure"),
	bserv::make_db_field<std::string>("destination"),
	bserv::make_db_field<std::string>("dept_time"),
	bserv::make_db_field<std::string>("dept_ap"),
	bserv::make_db_field<std::string>("arrv_time"),
	bserv::make_db_field<std::string>("arrv_ap"),
	bserv::make_db_field<std::string>("airline"),
	bserv::make_db_field<std::string>("price"),
	bserv::make_db_field<std::int64_t>("total_seat"),
	bserv::make_db_field<std::int64_t>("available_seat")
};

bserv::db_relation_to_object orm_order{
	bserv::make_db_field<std::string>("flight_number"),
	bserv::make_db_field<std::string>("departure"),
	bserv::make_db_field<std::string>("destination"),
	bserv::make_db_field<std::string>("dept_time"),
	bserv::make_db_field<std::string>("arrv_time"),
	bserv::make_db_field<std::string>("airline"),
	bserv::make_db_field<std::string>("price"),
	bserv::make_db_field<std::string>("username"),
};

std::optional<boost::json::object> get_user(
	bserv::db_transaction& tx,
	const boost::json::string& username) {
	bserv::db_result r = tx.exec(
		"select * from auth_user where username = ?", username);
	lginfo << r.query(); // this is how you log info
	return orm_user.convert_to_optional(r);
}

std::string get_or_empty(
	boost::json::object& obj,
	const std::string& key) {
	return obj.count(key) ? obj[key].as_string().c_str() : "";
}

// if you want to manually modify the response,
// the return type should be `std::nullopt_t`,
// and the return value should be `std::nullopt`.
std::nullopt_t hello(
	bserv::response_type& response,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	boost::json::object obj;
	if (session.count("user")) {
		// NOTE: modifications to sessions must be performed
		// BEFORE referencing objects in them. this is because
		// modifications might invalidate referenced objects.
		// in this example, "count" might be added to `session`,
		// which should be performed first.
		// then `user` can be referenced safely.
		if (!session.count("count")) {
			session["count"] = 0;
		}
		auto& user = session["user"].as_object();
		session["count"] = session["count"].as_int64() + 1;
		obj = {
			{"welcome", user["username"]},
			{"count", session["count"]}
		};
	}
	else {
		obj = { {"msg", "hello, world!"} };
	}
	// the response body is a string,
	// so the `obj` should be serialized
	response.body() = boost::json::serialize(obj);
	response.prepare_payload(); // this line is important!
	return std::nullopt;
}

// if you return a json object, the serialization
// is performed automatically.
boost::json::object user_register(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params["username"].as_string() == "") {
		return {
			{"success", false},
			{"message", "`Username` is required"}
		};
	}
	if (params["password"].as_string() == "") {
		return {
			{"success", false},
			{"message", "`Password` is required"}
		};
	}
	auto username = params["username"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`Username` already existed"}
		};
	}
	auto password = params["password"].as_string();
	bserv::db_result r = tx.exec(
		"insert into ? "
		"(?, password, is_superuser, "
		"first_name, last_name, phone_number, is_active) values "
		"(?, ?, ?, ?, ?, ?, ?)", bserv::db_name("auth_user"),
		bserv::db_name("username"),
		username,
		bserv::utils::security::encode_password(
			password.c_str()), false,
		get_or_empty(params, "first_name"),
		get_or_empty(params, "last_name"),
		get_or_empty(params, "phone_number"), true);
	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "User successfully registered"}
	};
}

boost::json::object user_update(
	bserv::request_type& request,
	std::shared_ptr<bserv::session_type> session_ptr,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	bserv::session_type& session = *session_ptr;
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	lgdebug << params;
	if (params["username"].as_string() == "") {
		return {
			{"success", false},
			{"message", "`Username` is required"}
		};
	}
	if (params["password"].as_string() == "") {
		return {
			{"success", false},
			{"message", "`Password` is required"}
		};
	}
	if (params["password"].as_string() != params["password_repeat"].as_string()) {
		return {
			{"success", false},
			{"message", "'Password' wrongly repeated"}
		};
	}
	auto user = session["user"].as_object();
	auto userid = boost::json::value_to<std::int64_t>(user["id"]);
	auto username = boost::json::value_to<std::string>(user["username"]);
	bserv::db_transaction tx{ conn };
	auto un = params["username"].as_string();
	auto opt_user = get_user(tx, un);
	if (opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`Username` already existed"}
		};
	}
	auto password = params["password"].as_string();
	tx.exec("update auth_user set username = ?, password = ?, first_name = ?, last_name = ?, phone_number = ? where id = ?;", params["username"], bserv::utils::security::encode_password(password.c_str()), get_or_empty(params, "first_name"), get_or_empty(params, "last_name"), get_or_empty(params, "phone_number"), userid);
	tx.exec("update orders set username = ? where username = ?;", params["username"], username);
	tx.commit(); // you must manually commit changes
	user["username"] = params["username"];
	user["first_name"] = get_or_empty(params, "first_name");
	user["last_name"] = get_or_empty(params, "last_name");
	user["phone_number"] = get_or_empty(params, "phone_number");
	session["user"] = user;
	if (user["is_superuser"].as_bool() == true)
		return {
			{"admin", true},
			{"success", true},
			{"message", "Personal Infomation successfully updated!"}
		};
	else
		return {
			{"success", true},
			{"message", "Personal Infomation successfully updated!"}
		};
}

boost::json::object user_login(
	bserv::request_type& request,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params["username"].as_string() == "") {
		return {
			{"success", false},
			{"message", "`Username` is required"}
		};
	}
	if (params["password"].as_string() == "") {
		return {
			{"success", false},
			{"message", "`Password` is required"}
		};
	}
	auto username = params["username"].as_string();
	std::string username_string = boost::json::value_to<std::string>(params["username"]);
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "Invalid username/password"}
		};
	}
	auto& user = opt_user.value();
	if (!user["is_active"].as_bool()) {
		return {
			{"success", false},
			{"message", "Invalid username/password"}
		};
	}
	auto password = params["password"].as_string();
	auto encoded_password = user["password"].as_string();
	if (!bserv::utils::security::check_password(
		password.c_str(), encoded_password.c_str())) {
		return {
			{"success", false},
			{"message", "Invalid username/password"}
		};
	}
	bserv::session_type& session = *session_ptr;
	session["user"] = user;
	if (user["is_superuser"] == true) {
		return {
			{"success", true},
			{"message", "Welcome, " + username_string + "!"},
			{"admin", true}
		};
	}
	else {
		return {
			{"success", true},
			{"message", "Welcome, " + username_string + "!"}
		};
	}
}

boost::json::object find_user(
	std::shared_ptr<bserv::db_connection> conn,
	const std::string& username) {
	bserv::db_transaction tx{ conn };
	auto user = get_user(tx, username.c_str());
	if (!user.has_value()) {
		return {
			{"success", false},
			{"message", "Requested user does not exist"}
		};
	}
	user.value().erase("id");
	user.value().erase("password");
	return {
		{"success", true},
		{"user", user.value()}
	};
}

boost::json::object user_logout(
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	if (session.count("user")) {
		session.erase("user");
	}
	return {
		{"success", true},
		{"message", "Logout successfully"}
	};
}

boost::json::object send_request(
	std::shared_ptr<bserv::session_type> session,
	std::shared_ptr<bserv::http_client> client_ptr,
	boost::json::object&& params) {
	// post for response:
	// auto res = client_ptr->post(
	//     "localhost", "8080", "/echo", {{"msg", "request"}}
	// );
	// return {{"response", boost::json::parse(res.body())}};
	// -------------------------------------------------------
	// - if it takes longer than 30 seconds (by default) to
	// - get the response, this will raise a read timeout
	// -------------------------------------------------------
	// post for json response (json value, rather than json
	// object, is returned):
	auto obj = client_ptr->post_for_value(
		"localhost", "8080", "/echo", { {"request", params} }
	);
	if (session->count("cnt") == 0) {
		(*session)["cnt"] = 0;
	}
	(*session)["cnt"] = (*session)["cnt"].as_int64() + 1;
	return { {"response", obj}, {"cnt", (*session)["cnt"]} };
}

boost::json::object echo(
	boost::json::object&& params) {
	return { {"echo", params} };
}

// websocket
std::nullopt_t ws_echo(
	std::shared_ptr<bserv::session_type> session,
	std::shared_ptr<bserv::websocket_server> ws_server) {
	ws_server->write_json((*session)["cnt"]);
	while (true) {
		try {
			std::string data = ws_server->read();
			ws_server->write(data);
		}
		catch (bserv::websocket_closed&) {
			break;
		}
	}
	return std::nullopt;
}

std::nullopt_t serve_static_files(
	bserv::response_type& response,
	const std::string& path) {
	return serve(response, path);
}

std::nullopt_t index(
	const std::string& template_path,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object& context) {
	bserv::session_type& session = *session_ptr;
	if (session.contains("user")) {
		context["user"] = session["user"];
	}
	lgdebug << context;
	return render(response, template_path, context);
}

std::nullopt_t form_login(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	lgdebug << params << std::endl;
	auto context = user_login(request, std::move(params), conn, session_ptr);
	lginfo << "login: " << context << std::endl;
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t form_logout(
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response) {
	auto context = user_logout(session_ptr);
	lginfo << "logout: " << context << std::endl;
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t update_user_info(
	bserv::request_type& request,
	std::shared_ptr<bserv::db_connection> conn,
	boost::json::object&& params,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response) {
	boost::json::object context = user_update(request, session_ptr, std::move(params), conn);
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_users(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context) {
	lgdebug << "view users: " << page_id << std::endl;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from auth_user;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from auth_user order by is_superuser desc limit 10 offset ?;", (page_id - 1) * 10);
	lginfo << db_res.query();
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["users"] = json_users;
	return index("users.html", session_ptr, response, context);
}

std::nullopt_t all_flights(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context) {
	bserv::session_type& session = *session_ptr;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	if (session.contains("user")) {
		auto user = session["user"].as_object();
		auto username = boost::json::value_to<std::string>(user["username"]);
		db_res = tx.exec("select count(*) from flightinfo where flight_number not in (select flight_number from orders where username = ?);", username);
	}		
	else
		db_res = tx.exec("select count(*) from flightinfo;");
	std::size_t total_flights = (*db_res.begin())[0].as<std::size_t>();
	int total_pages = (int)total_flights / 10;
	if (total_flights % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	if (session.contains("user")) {
		auto user = session["user"].as_object();
		auto username = boost::json::value_to<std::string>(user["username"]);
		db_res = tx.exec("select * from flightinfo where flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", username, (page_id - 1) * 10);
	}
	else
		db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
	lginfo << db_res.query();
	auto flights = orm_flight.convert_to_vector(db_res);
	boost::json::array json_flights;
	for (auto& flight : flights) {
		json_flights.push_back(flight);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["flights"] = json_flights;
	lgdebug << context;
	return index("flights.html", session_ptr, response, context);
}

std::nullopt_t all_flights_admin(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context) {
	bserv::session_type& session = *session_ptr;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from flightinfo;");
	std::size_t total_flights = (*db_res.begin())[0].as<std::size_t>();
	int total_pages = (int)total_flights / 10;
	if (total_flights % 10 != 0) ++total_pages;
	db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
	lginfo << db_res.query();
	auto flights = orm_flight.convert_to_vector(db_res);
	boost::json::array json_flights;
	for (auto& flight : flights) {
		json_flights.push_back(flight);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["flights"] = json_flights;
	return index("flights_admin.html", session_ptr, response, context);
}

std::nullopt_t all_orders(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context) {
	bserv::session_type& session = *session_ptr;
	bserv::db_transaction tx{ conn };
	auto user = session["user"].as_object();
	lgdebug << user;
	auto username = user["username"].as_string();
	auto is_superuser = user["is_superuser"].as_bool();
	bserv::db_result db_res;
	int total_pages;
	boost::json::array json_orders;
	if (is_superuser == true) {
		db_res = tx.exec("select count(*) from orders;");
		std::size_t total_orders = (*db_res.begin())[0].as<std::size_t>();
		total_pages = (int)total_orders / 10;
		if (total_orders % 10 != 0) ++total_pages;
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number order by f.dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
		auto orders = orm_order.convert_to_vector(db_res);
		for (auto& order : orders) {
			json_orders.push_back(order);
		}
		lgdebug << json_orders;
	}
	else {
		db_res = tx.exec("select count(*) from orders where username = ?;", username);
		std::size_t total_orders = (*db_res.begin())[0].as<std::size_t>();
		total_pages = (int)total_orders / 10;
		if (total_orders % 10 != 0) ++total_pages;
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number order by f.dept_time asc limit 10 offset ? ; "
			, username, (page_id - 1) * 10);
		auto orders = orm_flight.convert_to_vector(db_res);
		for (auto& order : orders) {
			json_orders.push_back(order);
		}
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["orders"] = json_orders;
	if (is_superuser == true) {
		context["admin"] = true;
		return index("orders.html", session_ptr, response, context);
	}
	else return index("myorders.html", session_ptr, response, context);
}

std::nullopt_t index_page(
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response) {
	bserv::session_type& session = *session_ptr;
	boost::json::object context;
	boost::json::object user;
	if (session.contains("user")) {
		boost::json::object user = session["user"].as_object();
		if (user["is_superuser"].as_bool() == true)
			context["admin"] = true;
	}
	lgdebug << context;
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t view_flights(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return all_flights(conn, session_ptr, response, page_id, std::move(context));
}

std::nullopt_t view_flights_admin(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context = {
		{"admin", true}
	};
	return all_flights_admin(conn, session_ptr, response, page_id, std::move(context));
}

std::nullopt_t view_orders(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return all_orders(conn, session_ptr, response, page_id, std::move(context));
}

std::nullopt_t view_users(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context = {{"admin", true}};
	return redirect_to_users(conn, session_ptr, response, page_id, std::move(context));
}

std::nullopt_t alter_user_status(
	std::shared_ptr<bserv::db_connection> conn,
	boost::json::object&& params,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response) {
	bserv::session_type& session = *session_ptr;
	bserv::db_transaction tx{ conn };
	lgdebug << params;
	auto userid = params["id"].as_string();
	auto is_active = params["is_active"].as_string();
	if (is_active == "true")
		tx.exec("update auth_user set is_active = false where id = ?", userid);
	else
		tx.exec("update auth_user set is_active = true where id = ?", userid);
	boost::json::object context = { {"admin", true} };
	int page_id = 1;
	bserv::db_result db_res = tx.exec("select count(*) from auth_user;");
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from auth_user order by is_superuser desc limit 10 offset ?;", (page_id - 1) * 10);
	tx.commit();
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["users"] = json_users;
	return index("users.html", session_ptr, response, context);
}

std::nullopt_t form_add_user(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = user_register(request, std::move(params), conn);
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t search_flights(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	const std::string& page_num) {
	bserv::session_type& session = *session_ptr;
	bserv::db_transaction tx{ conn };
	lgdebug << params;
	auto departure = params["departure"].as_string();
	auto destination = params["destination"].as_string();
	auto airline = params["airline"].as_string();
	int page_id = std::stoi(page_num);
	bserv::db_result db_res;
	std::size_t total_flights;
	lgdebug << session;
	if (session.contains("user")) {
		auto user = session["user"].as_object();
		auto uname = user["username"].as_string();
		auto is_superuser = user["is_superuser"].as_bool();
		if (is_superuser == true) {
			if (departure == "" && destination == "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo;");
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
			}
			if (departure == "" && destination == "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where airline = ?;", airline);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where airline = ? order by dept_time asc limit 10 offset ?;", airline, (page_id - 1) * 10);
			}
			if (departure == "" && destination != "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where destination = ?;", destination);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where destination = ? order by dept_time asc limit 10 offset ?;", destination, (page_id - 1) * 10);
			}
			if (departure == "" && destination != "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where destination = ? and airline = ?;", destination, airline);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where destination = ? and airline = ? order by dept_time asc limit 10 offset ?;", destination, airline, (page_id - 1) * 10);
			}
			if (departure != "" && destination == "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ?;", departure);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? order by dept_time limit 10 offset ?;", departure, (page_id - 1) * 10);
			}
			if (departure != "" && destination == "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and airline = ?;", departure, airline);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and airline = ? order by dept_time asc limit 10 offset ?;", departure, airline, (page_id - 1) * 10);
			}
			if (departure != "" && destination != "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and destination = ?;", departure, destination);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and destination = ? order by dept_time asc limit 10 offset ?;", departure, destination, (page_id - 1) * 10);
			}
			if (departure != "" && destination != "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and destination = ? and airline = ?;", departure, destination, airline);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and destination = ? and airline = ? order by dept_time asc limit 10 offset ?;", departure, destination, airline, (page_id - 1) * 10);
			}
		}
		else {
			if (departure == "" && destination == "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where flight_number not in (select flight_number from orders where username = ?);", uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", uname, (page_id - 1) * 10);
			}
			if (departure == "" && destination == "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where airline = ? and flight_number not in (select flight_number from orders where username = ?);", airline, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where airline = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", airline, uname, (page_id - 1) * 10);
			}
			if (departure == "" && destination != "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where destination = ? and flight_number not in (select flight_number from orders where username = ?);", destination, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where destination = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", destination, uname, (page_id - 1) * 10);
			}
			if (departure == "" && destination != "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where destination = ? and airline = ? and flight_number not in (select flight_number from orders where username = ?);", destination, airline, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where destination = ? and airline = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", destination, airline, uname, (page_id - 1) * 10);
			}
			if (departure != "" && destination == "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and flight_number not in (select flight_number from orders where username = ?);", departure, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time limit 10 offset ?;", departure, uname, (page_id - 1) * 10);
			}
			if (departure != "" && destination == "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and airline = ? and flight_number not in (select flight_number from orders where username = ?);", departure, airline, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and airline = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", departure, airline, uname, (page_id - 1) * 10);
			}
			if (departure != "" && destination != "" && airline == "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and destination = ? and flight_number not in (select flight_number from orders where username = ?);", departure, destination, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and destination = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", departure, destination, uname, (page_id - 1) * 10);
			}
			if (departure != "" && destination != "" && airline != "") {
				db_res = tx.exec("select count(*) from flightinfo where departure = ? and destination = ? and airline = ? and flight_number not in (select flight_number from orders where username = ?);", departure, destination, airline, uname);
				total_flights = (*db_res.begin())[0].as<std::size_t>();
				db_res = tx.exec("select * from flightinfo where departure = ? and destination = ? and airline = ? and flight_number not in (select flight_number from orders where username = ?) order by dept_time asc limit 10 offset ?;", departure, destination, airline, uname, (page_id - 1) * 10);
			}
		}
	}
	else {
		if (departure == "" && destination == "" && airline == "") {
			db_res = tx.exec("select count(*) from flightinfo;");
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
		}
		if (departure == "" && destination == "" && airline != "") {
			db_res = tx.exec("select count(*) from flightinfo where airline = ?;", airline);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where airline = ? order by dept_time asc limit 10 offset ?;", airline, (page_id - 1) * 10);
		}
		if (departure == "" && destination != "" && airline == "") {
			db_res = tx.exec("select count(*) from flightinfo where destination = ?;", destination);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where destination = ? order by dept_time asc limit 10 offset ?;", destination, (page_id - 1) * 10);
		}
		if (departure == "" && destination != "" && airline != "") {
			db_res = tx.exec("select count(*) from flightinfo where destination = ? and airline = ?;", destination, airline);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where destination = ? and airline = ? order by dept_time asc limit 10 offset ?;", destination, airline, (page_id - 1) * 10);
		}
		if (departure != "" && destination == "" && airline == "") {
			db_res = tx.exec("select count(*) from flightinfo where departure = ?;", departure);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where departure = ? order by dept_time limit 10 offset ?;", departure, (page_id - 1) * 10);
		}
		if (departure != "" && destination == "" && airline != "") {
			db_res = tx.exec("select count(*) from flightinfo where departure = ? and airline = ?;", departure, airline);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where departure = ? and airline = ? order by dept_time asc limit 10 offset ?;", departure, airline, (page_id - 1) * 10);
		}
		if (departure != "" && destination != "" && airline == "") {
			db_res = tx.exec("select count(*) from flightinfo where departure = ? and destination = ?;", departure, destination);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where departure = ? and destination = ? order by dept_time asc limit 10 offset ?;", departure, destination, (page_id - 1) * 10);
		}
		if (departure != "" && destination != "" && airline != "") {
			db_res = tx.exec("select count(*) from flightinfo where departure = ? and destination = ? and airline = ?;", departure, destination, airline);
			total_flights = (*db_res.begin())[0].as<std::size_t>();
			db_res = tx.exec("select * from flightinfo where departure = ? and destination = ? and airline = ? order by dept_time asc limit 10 offset ?;", departure, destination, airline, (page_id - 1) * 10);
		}
	}
	int total_pages = (int)total_flights / 10;
	if (total_flights % 10 != 0) ++total_pages;
	auto flights = orm_flight.convert_to_vector(db_res);
	boost::json::array json_flights;
	for (auto& flight : flights) {
		json_flights.push_back(flight);
	}
	boost::json::object pagination;
	boost::json::object context;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		lgdebug << context;
		context["pagination"] = pagination;
		lgdebug << context;
	}
	context["flights"] = json_flights;
	std::string dep = boost::json::value_to<std::string>(params["departure"]);
	std::string dest = boost::json::value_to<std::string>(params["destination"]);
	std::string airl = boost::json::value_to<std::string>(params["airline"]);
	context["departure"] = bserv::utils::encode_url(dep);
	context["destination"] = bserv::utils::encode_url(dest);
	context["airline"] = bserv::utils::encode_url(airl);
	lgdebug << context;
	if (session.contains("user")) {
		auto user = session["user"].as_object();
		auto is_superuser = user["is_superuser"].as_bool();
		if (is_superuser == true) {
			context["admin"] = true;
			return index("flights_admin_search.html", session_ptr, response, context);
		}
		else return index("flights_search.html", session_ptr, response, context);
	}
	else return index("flights_search.html", session_ptr, response, context);
}

std::nullopt_t make_purchase(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	bserv::session_type& session = *session_ptr;
	boost::json::object context;
	lgdebug << params;
	auto user = session["user"].as_object();
	auto username = user["username"].as_string();
	auto flight_number = params["flight_number"].as_string();
	auto available_seat = params["available_seat"].as_string();
	if (available_seat != "0") {
		bserv::db_transaction tx{ conn };
		bserv::db_result db_res;
		db_res = tx.exec("insert into orders(username, flight_number) values(?, ?);", username, flight_number);
		tx.exec("update flightinfo set available_seat = available_seat - 1 where flight_number = ?", flight_number);
		tx.commit();
		context = {
			{"success", true},
			{"message", "Order successfully made!"}
		};
	}
	else {
		context = {
			{"success", false},
			{"message", "No available seat!"}
		};
	}
	return all_flights(conn, session_ptr, response, 1, std::move(context));
}

std::nullopt_t search_myorders(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	const std::string& page_num) {
	bserv::session_type& session = *session_ptr;
	boost::json::object context;
	auto user = session["user"].as_object();
	auto username = user["username"].as_string();
	bserv::db_transaction tx{ conn };
	auto departure = params["departure"].as_string();
	auto destination = params["destination"].as_string();
	auto airline = params["airline"].as_string();
	int page_id = std::stoi(page_num);
	bserv::db_result db_res;
	std::size_t total_orders;
	if (departure == "" && destination == "" && airline == "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number;", username);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number order by f.dept_time asc limit 10 offset ?;", username, (page_id - 1) * 10);
	}
	if (departure == "" && destination == "" && airline != "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.airline = ?;", username, airline);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.airline = ? order by f.dept_time asc limit 10 offset ?;", username, airline, (page_id - 1) * 10);
	}
	if (departure == "" && destination != "" && airline == "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.destination = ?;", username, destination);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.destination = ? order by f.dept_time asc limit 10 offset ?;", username, destination, (page_id - 1) * 10);
	}
	if (departure == "" && destination != "" && airline != "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.destination = ? and f.airline = ?;", username, destination, airline);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.destination = ? and f.airline = ? order by f.dept_time asc limit 10 offset ?;", username, destination, airline, (page_id - 1) * 10);
	}
	if (departure != "" && destination == "" && airline == "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ?;", username, departure);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? order by f.dept_time asc limit 10 offset ?;", username, departure, (page_id - 1) * 10);
	}
	if (departure != "" && destination == "" && airline != "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? and f.airline = ?;", username, departure, airline);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? and f.airline = ? order by f.dept_time asc limit 10 offset ?;", username, departure, airline, (page_id - 1) * 10);
	}
	if (departure != "" && destination != "" && airline == "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? and f.destination = ?;", username, departure, destination);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? and f.destination = ? order by f.dept_time asc limit 10 offset ?;", username, departure, destination, (page_id - 1) * 10);
	}
	if (departure != "" && destination != "" && airline != "") {
		db_res = tx.exec("select count(*) from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? and f.destination = ? and f.airline = ?;", username, departure, destination, airline);
		total_orders = (*db_res.begin())[0].as<std::size_t>();
		db_res = tx.exec("select f.id, f.flight_number, f.departure, f.destination, f.dept_time, f.dept_ap, f.arrv_time, f.arrv_ap, f.airline, f.price, "
		"f.total_seat, f.available_seat from orders o, flightinfo f where o.username = ? and o.flight_number = f.flight_number and f.departure = ? and f.destination = ? and f.airline = ? order by f.dept_time asc limit 10 offset ?;", username, departure, destination, airline, (page_id - 1) * 10);
	}
	int total_pages = (int)total_orders / 10;
	if (total_orders % 10 != 0) ++total_pages;
	auto orders = orm_flight.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["orders"] = json_orders;
	std::string dep = boost::json::value_to<std::string>(params["departure"]);
	std::string dest = boost::json::value_to<std::string>(params["destination"]);
	std::string airl = boost::json::value_to<std::string>(params["airline"]);
	context["departure"] = bserv::utils::encode_url(dep);
	context["destination"] = bserv::utils::encode_url(dest);
	context["airline"] = bserv::utils::encode_url(airl);
	lgdebug << context;
	return index("myorders_search.html", session_ptr, response, context);
}

std::nullopt_t cancel_orders(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	bserv::db_transaction tx{ conn };
	lgdebug << params;
	auto flight_number = params["flight_number"].as_string();
	bserv::session_type& session = *session_ptr;
	boost::json::object context;
	auto user = session["user"].as_object();
	auto username = user["username"].as_string();
	bserv::db_result db_res;
	db_res = tx.exec("delete from orders where username = ? and flight_number = ?;", username, flight_number);
	tx.exec("update flightinfo set available_seat = available_seat + 1 where flight_number = ?", flight_number);
	tx.commit();
	context = {
		{"success", true},
		{"message", "Order successfully cancelled!"}
	};
	return all_orders(conn, session_ptr, response, 1, std::move(context));
}

std::nullopt_t reset_flights_admin(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	boost::json::object&& params,
	bserv::response_type& response) {
	lgdebug << params;
	auto id = params["id"].as_string();
	auto flight_number = params["flight_number"].as_string();
	auto departure = params["departure"].as_string();
	auto destination = params["destination"].as_string();
	auto dept_time = params["departure_time"].as_string();
	auto dept_ap = params["departure_airport"].as_string();
	auto arrv_time = params["arrival_time"].as_string();
	auto arrv_ap = params["arrival_airport"].as_string();
	auto airline = params["airline"].as_string();
	auto price = params["price"].as_string();
	auto total_seat_before = boost::json::value_to<std::string>(params["total_seat_before"]);
	auto total_seat = boost::json::value_to<std::string>(params["total_seat"]);
	auto available_seat_before = boost::json::value_to<std::string>(params["available_seat"]);
	int tsb = std::stoi(total_seat_before);
	int ts = std::stoi(total_seat);
	int as = std::stoi(available_seat_before);
	auto available_seat = as - tsb + ts;
	boost::json::object context;
	if (tsb - ts > as)
		context = { {"admin", true}, {"success", false}, {"message", "The number of seat can't be smaller than existing orders"} };
	else {
		bserv::db_transaction tx{ conn };
		tx.exec("update flightinfo set flight_number = ?, departure = ?, destination = ?, dept_time = ?,"
			"dept_ap = ?, arrv_time = ?, arrv_ap = ?, airline = ?, price = ?, total_seat = ?, available_seat = ? where id = ?;"
			, flight_number, departure, destination, dept_time, dept_ap, arrv_time, arrv_ap, airline, price, total_seat, available_seat, id);
		tx.commit();
		context = { {"admin", true}, {"success", true}, {"message", "Flight Infomation successfully reset!"} };
	}
	return all_flights_admin(conn, session_ptr, response, 1, std::move(context));
}

std::nullopt_t cancel_flights_admin(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	boost::json::object&& params,
	bserv::response_type& response) {
	auto flight_number = params["flight_number"];
	boost::json::object context;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select count(*) from orders where flight_number = ?;", flight_number);
	std::size_t total_orders = (*db_res.begin())[0].as<std::size_t>();
	int page_id = 1;
	if (total_orders != 0) {
		context = { {"admin", true}, {"success", false}, {"message", "You can't cancel a flight already ordered."} };
		bserv::session_type& session = *session_ptr;
		bserv::db_result db_res = tx.exec("select count(*) from flightinfo;");
		std::size_t total_flights = (*db_res.begin())[0].as<std::size_t>();
		int total_pages = (int)total_flights / 10;
		if (total_flights % 10 != 0) ++total_pages;
		db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
		auto flights = orm_flight.convert_to_vector(db_res);
		boost::json::array json_flights;
		for (auto& flight : flights) {
			json_flights.push_back(flight);
		}
		boost::json::object pagination;
		if (total_pages != 0) {
			pagination["total"] = total_pages;
			if (page_id > 1) {
				pagination["previous"] = page_id - 1;
			}
			if (page_id < total_pages) {
				pagination["next"] = page_id + 1;
			}
			int lower = page_id - 3;
			int upper = page_id + 3;
			if (page_id - 3 > 2) {
				pagination["left_ellipsis"] = true;
			}
			else {
				lower = 1;
			}
			if (page_id + 3 < total_pages - 1) {
				pagination["right_ellipsis"] = true;
			}
			else {
				upper = total_pages;
			}
			pagination["current"] = page_id;
			boost::json::array pages_left;
			for (int i = lower; i < page_id; ++i) {
				pages_left.push_back(i);
			}
			pagination["pages_left"] = pages_left;
			boost::json::array pages_right;
			for (int i = page_id + 1; i <= upper; ++i) {
				pages_right.push_back(i);
			}
			pagination["pages_right"] = pages_right;
			context["pagination"] = pagination;
		}
		context["flights"] = json_flights;
		return index("flights_admin.html", session_ptr, response, context);
	}
	else {
		tx.exec("delete from flightinfo where flight_number = ?", flight_number);
		context = { {"admin", true}, {"success", true}, {"message", "Flight successfully cancelled!"} };
		bserv::session_type& session = *session_ptr;
		bserv::db_result db_res = tx.exec("select count(*) from flightinfo;");
		std::size_t total_flights = (*db_res.begin())[0].as<std::size_t>();
		int total_pages = (int)total_flights / 10;
		if (total_flights % 10 != 0) ++total_pages;
		db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
		tx.commit();
		auto flights = orm_flight.convert_to_vector(db_res);
		boost::json::array json_flights;
		for (auto& flight : flights) {
			json_flights.push_back(flight);
		}
		boost::json::object pagination;
		if (total_pages != 0) {
			pagination["total"] = total_pages;
			if (page_id > 1) {
				pagination["previous"] = page_id - 1;
			}
			if (page_id < total_pages) {
				pagination["next"] = page_id + 1;
			}
			int lower = page_id - 3;
			int upper = page_id + 3;
			if (page_id - 3 > 2) {
				pagination["left_ellipsis"] = true;
			}
			else {
				lower = 1;
			}
			if (page_id + 3 < total_pages - 1) {
				pagination["right_ellipsis"] = true;
			}
			else {
				upper = total_pages;
			}
			pagination["current"] = page_id;
			boost::json::array pages_left;
			for (int i = lower; i < page_id; ++i) {
				pages_left.push_back(i);
			}
			pagination["pages_left"] = pages_left;
			boost::json::array pages_right;
			for (int i = page_id + 1; i <= upper; ++i) {
				pages_right.push_back(i);
			}
			pagination["pages_right"] = pages_right;
			context["pagination"] = pagination;
		}
		context["flights"] = json_flights;
		return index("flights_admin.html", session_ptr, response, context);
	}
}

std::nullopt_t add_flights_admin(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	boost::json::object&& params,
	bserv::response_type& response) {
	int page_id = 1;
	boost::json::object context;
	auto flight_number = params["flight_number"].as_string();
	auto departure = params["departure"].as_string();
	auto destination = params["destination"].as_string();
	auto dept_time = params["departure_time"].as_string();
	auto dept_ap = params["departure_airport"].as_string();
	auto arrv_time = params["arrival_time"].as_string();
	auto arrv_ap = params["arrival_airport"].as_string();
	auto airline = params["airline"].as_string();
	auto price = params["price"].as_string();
	auto total_seat = params["total_seat"].as_int64();
	bserv::db_transaction tx{ conn };
	tx.exec("insert into flightinfo(flight_number, departure, destination, dept_time, dept_ap, arrv_time, arrv_ap, airline, price, total_seat, available_seat) values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
		, flight_number, departure, destination, dept_time, dept_ap, arrv_time, arrv_ap, airline, price, total_seat, total_seat);
	context = { {"admin", true}, {"success", true}, {"message", "New flight successfully added!"} };
	bserv::db_result db_res = tx.exec("select count(*) from flightinfo;");
	std::size_t total_flights = (*db_res.begin())[0].as<std::size_t>();
	int total_pages = (int)total_flights / 10;
	if (total_flights % 10 != 0) ++total_pages;
	db_res = tx.exec("select * from flightinfo order by dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
	tx.commit();
	auto flights = orm_flight.convert_to_vector(db_res);
	boost::json::array json_flights;
	for (auto& flight : flights) {
		json_flights.push_back(flight);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["flights"] = json_flights;
	return index("flights_admin.html", session_ptr, response, context);
}

std::nullopt_t delete_orders(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	boost::json::object&& params,
	bserv::response_type& response) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto flight_number = params["flight_number"].as_string();
	auto username = params["username"].as_string();
	bserv::db_transaction tx{ conn };
	tx.exec("delete from orders where flight_number = ? and username = ?", flight_number, username);
	tx.exec("update flightinfo set available_seat = available_seat + 1 where flight_number = ?;", flight_number);
	int page_id = 1, total_pages;
	boost::json::array json_orders;
	bserv::db_result db_res;
	db_res = tx.exec("select count(*) from orders;");
	std::size_t total_orders = (*db_res.begin())[0].as<std::size_t>();
	total_pages = (int)total_orders / 10;
	if (total_orders % 10 != 0) ++total_pages;
	db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number order by f.dept_time asc limit 10 offset ?;", (page_id - 1) * 10);
	auto orders = orm_order.convert_to_vector(db_res);
	tx.commit();
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	lgdebug << json_orders;
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	context["success"] = true;
	context["message"] = "Order successfully deleted!";
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_flight_number(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto flight_number = params["flight_number"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number and f.flight_number = ? order by f.dept_time asc;", flight_number);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_departure(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto departure = params["departure"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number and f.departure = ? order by f.dept_time asc;", departure);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_destination(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto destination = params["destination"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number and f.destination = ? order by f.dept_time asc;", destination);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_airline(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto airline = params["airline"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number and f.airline = ? order by f.dept_time asc;", airline);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_o_username(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto username = params["username"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o, flightinfo f where o.flight_number = f.flight_number and o.username = ? order by f.dept_time asc;", username);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_dept_time(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto boa = params["boa"].as_string();
	auto dept_time = params["dept_time"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	if (boa != "before" && boa != "after") {
		context["admin"] = true;
		context["success"] = false;
		context["message"] = "Invalid input";
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o,"
			"flightinfo f where o.flight_number = f.flight_number order by f.dept_time asc;");
		auto orders = orm_order.convert_to_vector(db_res);
		boost::json::array json_orders;
		for (auto& order : orders) {
			json_orders.push_back(order);
		}
		context["orders"] = json_orders;
		return index("orders.html", session_ptr, response, context);
	}
	else if (boa == "before")
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o,"
			"flightinfo f where o.flight_number = f.flight_number and f.dept_time < ? order by f.dept_time asc;", dept_time);
	else
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o,"
			"flightinfo f where o.flight_number = f.flight_number and f.dept_time > ? order by f.dept_time asc;", dept_time);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_arrv_time(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto boa = params["boa"].as_string();
	auto arrv_time = params["arrv_time"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	if (boa != "before" && boa != "after") {
		context["admin"] = true;
		context["success"] = false;
		context["message"] = "Invalid input";
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o,"
			"flightinfo f where o.flight_number = f.flight_number order by f.dept_time asc;");
		auto orders = orm_order.convert_to_vector(db_res);
		boost::json::array json_orders;
		for (auto& order : orders) {
			json_orders.push_back(order);
		}
		context["orders"] = json_orders;
		return index("orders.html", session_ptr, response, context);
	}
	else if (boa == "before")
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o,"
			"flightinfo f where o.flight_number = f.flight_number and f.arrv_time < ? order by f.dept_time asc;", arrv_time);
	else
		db_res = tx.exec("select f.flight_number, f.departure, f.destination, f.dept_time, f.arrv_time, f.airline, f.price, o.username from orders o,"
			"flightinfo f where o.flight_number = f.flight_number and f.arrv_time > ? order by f.dept_time asc;", arrv_time);
	auto orders = orm_order.convert_to_vector(db_res);
	boost::json::array json_orders;
	for (auto& order : orders) {
		json_orders.push_back(order);
	}
	context["orders"] = json_orders;
	context["admin"] = true;
	return index("orders.html", session_ptr, response, context);
}

std::nullopt_t search_u_username(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto username = params["username"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select * from auth_user where username = ? order by is_superuser desc;", username);
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	context["users"] = json_users;
	context["admin"] = true;
	return index("users.html", session_ptr, response, context);
}

std::nullopt_t search_first_name(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto first_name = params["first_name"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select * from auth_user where first_name = ? order by is_superuser desc;", first_name);
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	context["users"] = json_users;
	context["admin"] = true;
	return index("users.html", session_ptr, response, context);
}

std::nullopt_t search_last_name(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto last_name = params["last_name"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select * from auth_user where last_name = ? order by is_superuser desc;", last_name);
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	context["users"] = json_users;
	context["admin"] = true;
	return index("users.html", session_ptr, response, context);
}

std::nullopt_t search_active(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	bserv::json::object context;
	lgdebug << params;
	auto is_active = params["is_active"].as_string();
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res;
	db_res = tx.exec("select * from auth_user where is_active = ? order by is_superuser desc;", is_active);
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	context["users"] = json_users;
	context["admin"] = true;
	return index("users.html", session_ptr, response, context);
}