#pragma once

#include <boost/json.hpp>

#include <string>
#include <memory>
#include <optional>

#include "bserv/common.hpp"

std::nullopt_t hello(
    bserv::response_type& response,
    std::shared_ptr<bserv::session_type> session_ptr);

boost::json::object user_register(
    bserv::request_type& request,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn);

boost::json::object user_login(
    bserv::request_type& request,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

boost::json::object find_user(
    std::shared_ptr<bserv::db_connection> conn,
    const std::string& username);

boost::json::object user_logout(
    std::shared_ptr<bserv::session_type> session_ptr);

boost::json::object send_request(
    std::shared_ptr<bserv::session_type> session,
    std::shared_ptr<bserv::http_client> client_ptr,
    boost::json::object&& params);

boost::json::object echo(
    boost::json::object&& params);

// websocket
std::nullopt_t ws_echo(
    std::shared_ptr<bserv::session_type> session,
    std::shared_ptr<bserv::websocket_server> ws_server);

std::nullopt_t serve_static_files(
    bserv::response_type& response,
    const std::string& path);

std::nullopt_t index_page(
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response);

std::nullopt_t view_flights(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response,
    const std::string& page_num);

std::nullopt_t view_flights_admin(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response,
    const std::string& page_num);

std::nullopt_t view_orders(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response,
    const std::string& page_num);

std::nullopt_t form_login(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t form_logout(
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response);

std::nullopt_t update_user_info(
    bserv::request_type& request,
    std::shared_ptr<bserv::db_connection> conn,
    boost::json::object&& params,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response);

std::nullopt_t view_users(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response,
    const std::string& page_num);

std::nullopt_t alter_user_status(
    std::shared_ptr<bserv::db_connection> conn,
    boost::json::object&& params,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response);

std::nullopt_t form_add_user(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_flights(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    const std::string& page_num);

std::nullopt_t make_purchase(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_myorders(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    const std::string& page_num);

std::nullopt_t search_flight_number(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_departure(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_destination(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_airline(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_o_username(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_u_username(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_dept_time(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_arrv_time(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_first_name(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_last_name(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t search_active(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t cancel_orders(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t reset_flights_admin(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    boost::json::object&& params,
    bserv::response_type& response);

std::nullopt_t cancel_flights_admin(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    boost::json::object&& params,
    bserv::response_type& response);

std::nullopt_t add_flights_admin(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    boost::json::object&& params,
    bserv::response_type& response);

std::nullopt_t delete_orders(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    boost::json::object&& params,
    bserv::response_type& response);
