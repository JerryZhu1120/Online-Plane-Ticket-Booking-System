CREATE TABLE auth_user (
    id serial PRIMARY KEY,
    username character varying(255) NOT NULL UNIQUE,
    password character varying(255) NOT NULL,
    is_superuser boolean NOT NULL,
    first_name character varying(255) NOT NULL,
    last_name character varying(255) NOT NULL,
    phone_number character varying(255) NOT NULL,
    is_active boolean NOT NULL
);

CREATE TABLE flightinfo (
    id serial PRIMARY KEY,
    flight_number character varying(255) NOT NULL UNIQUE,
    departure character varying(255) NOT NULL,
    destination character varying(255) NOT NULL,
    dept_time character varying(255) NOT NULL,
    dept_ap character varying(255) NOT NULL,
    arrv_time character varying(255) NOT NULL,
    arrv_ap character varying(255) NOT NULL,
    airline character varying(255) NOT NULL,
    price int NOT NULL,
    total_seat int,
    available_seat int
);

CREATE TABLE orders (
    username character varying(255) NOT NULL,
    flight_number character varying(255) NOT NULL
);