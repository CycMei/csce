#include <cstdlib>
#include <deque>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include "chat_message.h"



typedef std::deque<chat_message> chat_message_queue;

class chat_cliemt{
private:
	boost::asio::io_service &io_service_;
	boost::asio::ip::tcp::socket socket_;
	chat_message read_msg_;
	chat_message_queue write_msgs_;
private:
	void handle_connect(const boost::system::error_code &error) {
		if (!error){
			boost::asio::async_read(
				socket_, 
				boost::asio::buffer(read_msg_.data(), 
					chat_message::header_length),
				boost::bind(&chat_cliemt::handle_read_header,
					this,
					boost::asio::placeholders::error
					)
				);
		}
	}

	void handle_read_header(const boost::system::error_code &error) {
		if (!error && read_msg_.decode_header()){
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.body(),
					read_msg_.body_length()),
				boost::bind(&chat_cliemt::handle_read_body,
					this,
					boost::asio::placeholders::error)
				);
		}
		else{
			do_close();
		}
	}

	void handle_read_body(const boost::system::error_code &error) {
		if (!error){
			std::cout.write(read_msg_.body(),
				read_msg_.body_length());
			std::cout << "\n";
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.data(),
					chat_message::header_length),
				boost::bind(&chat_cliemt::handle_read_header,
					this,
					boost::asio::placeholders::error)
				);
		}
		else{
			do_close();
		}
	}

	void do_write(chat_message msg) {
		bool write_in_progress = !write_msgs_.empty();
		write_msgs_.push_back(msg);
		if (!write_in_progress){
			boost::asio::async_write(socket_,
				boost::asio::buffer(write_msgs_.front().data(),
					write_msgs_.front().length()),
				boost::bind(&chat_cliemt::handle_write,
					this,
					boost::asio::placeholders::error)
				);
		}
	}

	void handle_write(const boost::system::error_code &error) {
		if (!error){
			write_msgs_.pop_front();
			if (!write_msgs_.empty()){
				boost::asio::async_write(socket_,
					boost::asio::buffer(write_msgs_.front().data(),
						write_msgs_.front().length()),
					boost::bind(&chat_cliemt::handle_write,
						this,
						boost::asio::placeholders::error)
					);
			}
			else{
				do_close();
			}
		}
	}

	void do_close() {
		socket_.close();
	}

public:
	chat_cliemt(boost::asio::io_service &io_service,
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
	: io_service_(io_service), socket_(io_service){
		boost::asio::async_connect(socket_, 
			endpoint_iterator,
			boost::bind(&chat_cliemt::handle_connect,
				this,
				boost::asio::placeholders::error)
			);
	}

	void write(const chat_message &msg) {
		io_service_.post(boost::bind(&chat_cliemt::do_write,
			this,
			msg)
		);
	}

	void close() {
		io_service_.post(boost::bind(&chat_cliemt::do_close,
			this)
		);
	}

};



int main(int argc, char const *argv[])
{
	try{
		if (argc != 3){
			std::cerr << "usage: chat_cliemt <host> <post> \n";
			return 1;
		}

		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver(io_service);
		boost::asio::ip::tcp::resolver::query query(argv[1], argv[2]);
		boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
		chat_cliemt c(io_service, iterator)	;
		boost::thread t(boost::bind(&boost::asio::io_service::run,
			&io_service));
		char line[chat_message::max_body_length + 1];
		while(std::cin.getline(line,chat_message::max_body_length + 1) ) {

			chat_message msg;
			msg.body_length(std::strlen(line));
			std::memcpy(msg.body(), line, msg.body_length());
			msg.encode_header();
			c.write(msg);
		}

		c.close();
		t.join();

	}
	catch(std::exception &e){
		std::cerr << "exception: " << e.what() <<"\n";
	}

	return 0;
}




