#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include "chat_message.h"

typedef std::deque<chat_message> chat_message_queue;

class chat_paricipant{
public:
	virtual ~chat_paricipant(){}
	virtual void deliver(const chat_message &msg) = 0;
};

typedef boost::shared_ptr<chat_paricipant> chat_paricipant_ptr;


class chat_room{
private:
	std::set<chat_paricipant_ptr> participants_;
	enum {max_recent_msgs = 100};
	chat_message_queue recent_msgs_;
public:
	void join(chat_paricipant_ptr participant) {
		participants_.insert(participant);
		std::for_each(recent_msgs_.begin(),
			recent_msgs_.end(),
			boost::bind(&chat_paricipant::deliver,
				participant, _1));
	}

	void leave(chat_paricipant_ptr participant) {
		participants_.erase(participant);
	}

	void deliver(const chat_message &msg) {
		recent_msgs_.push_back(msg);
		while(recent_msgs_.size() > max_recent_msgs) {
			recent_msgs_.pop_front();
			std::for_each(participants_.begin(),
				participants_.end(),
				boost::bind(&chat_paricipant::deliver,
					_1,
					boost::ref(msg)));
		}
	}

};

class chat_session : public chat_paricipant, 
public boost::enable_shared_from_this<chat_session>{
private:
	boost::asio::ip::tcp::socket socket_;
	chat_room &room_;
	chat_message read_msg_;
	chat_message_queue write_msgs_;
public:
	chat_session(boost::asio::io_service &io_service,
		chat_room &room) : socket_(io_service), room_(room){

	}

	boost::asio::ip::tcp::socket &socket() {
		return socket_;
	}

	void start() {
		room_.join(shared_from_this());
		boost::asio::async_read(socket_,
			boost::asio::buffer(read_msg_.data(),
				chat_message::header_length),
			boost::bind(&chat_session::handle_read_header,
				shared_from_this(),
				boost::asio::placeholders::error));
	}

	void deliver(const chat_message &msg) {
		bool write_in_progress = !write_msgs_.empty();
		write_msgs_.push_back(msg);
		if (!write_in_progress) {
			boost::asio::async_write(socket_,
				boost::asio::buffer(write_msgs_.front().data(),
					write_msgs_.front().length()),
				boost::bind(&chat_session::handle_write,
					shared_from_this(),
					boost::asio::placeholders::error));
		}
	}

	void handle_read_header(const boost::system::error_code &error) {
		if (!error &&read_msg_.decode_header()) {
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.body(),
					read_msg_.body_length()),
				boost::bind(&chat_session::handle_read_body,
					shared_from_this(),
					boost::asio::placeholders::error));
		}
		else{
			room_.leave(shared_from_this());
		}
	}

	void handle_read_body(const boost::system::error_code &error) {
		if(!error) {
			room_.deliver(read_msg_);
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.data(),
					chat_message::header_length),
				boost::bind(&chat_session::handle_read_header,
					shared_from_this(),
					boost::asio::placeholders::error));
		}
		else{
			room_.leave(shared_from_this());
		}
	}

	void handle_write(const boost::system::error_code &error) {
		if(!error) {
			write_msgs_.pop_front();
			if(!write_msgs_.empty()){
				boost::asio::async_write(socket_,
					boost::asio::buffer(write_msgs_.front().data(),
						write_msgs_.front().length()),
					boost::bind(&chat_session::handle_write,
						shared_from_this(),
						boost::asio::placeholders::error));
			} 
			else{
				room_.leave(shared_from_this());
			}
		}
	}

};

typedef boost::shared_ptr<chat_session> chat_session_ptr;

class chat_server{
private:
	boost::asio::io_service &io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;
	chat_room room_;
public:
	chat_server(boost::asio::io_service &io_service,
		const boost::asio::ip::tcp::endpoint &endpoint):
	io_service_(io_service), acceptor_(io_service, endpoint) {
		start_accept();
	}

	void start_accept() {
		chat_session_ptr new_session(new chat_session(io_service_, room_));
		acceptor_.async_accept(new_session->socket(),
			boost::bind(&chat_server::handle_accept,
				this,
				new_session,
				boost::asio::placeholders::error))		;
	}

	void handle_accept(chat_session_ptr session,
		const boost::system::error_code &error) {
		if(!error) {
			session->start();
		}
		start_accept();
	}
};

typedef boost::shared_ptr<chat_server> chat_server_ptr;
typedef std::list<chat_server_ptr> chat_server_list;

int main(int argc, char const *argv[])
{
	try{
		if (argc < 2) {
			std::cerr <<"usage: chat_server <port> [<port> ..]\n";
			return 1;
		}
		boost::asio::io_service io_service;
		chat_server_list servers;
		for (int i = 1; i != argc; ++i) {
			boost::asio::ip::tcp::endpoint endpoint(
				boost::asio::ip::tcp::v4(),
				std::atoi(argv[i]));
			chat_server_ptr server(new chat_server(io_service,
				endpoint));
			// servers.push_back(server);
		}
		io_service.run();
	}
	catch(std::exception &e) {
		std::cerr << "exception: " << e.what() << "\n";
		throw;
	}
	return 0;
}