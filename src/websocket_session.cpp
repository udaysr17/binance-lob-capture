#include "websocket_session.h"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>
#include <iostream>

namespace bc {

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

bool run_websocket(const WsConfig &config,
                   const std::function<void(std::int64_t sec, std::int64_t nsec,
                                            std::string_view msg)> &on_message,
                   const std::function<bool()> &check_stop) {
  try {
    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    tcp::resolver resolver{ioc};
    websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};

    auto const results = resolver.resolve(config.host, config.port);
    net::connect(beast::get_lowest_layer(ws), results);

    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(),
                                  config.host.c_str())) {
      beast::error_code ec{static_cast<int>(::ERR_get_error()),
                           net::error::get_ssl_category()};
      throw beast::system_error{ec};
    }

    ws.next_layer().handshake(ssl::stream_base::client);

    ws.set_option(
        websocket::stream_base::decorator([](websocket::request_type &req) {
          req.set(http::field::user_agent,
                  BOOST_BEAST_VERSION_STRING " bc-capture");
        }));

    // Use zlib compression if supported
    websocket::permessage_deflate pmd;
    pmd.client_enable = true;
    pmd.server_enable = true;
    ws.set_option(pmd);

    ws.handshake(config.host, config.path);

    if (!config.subscribe_payload.empty()) {
      ws.write(net::buffer(std::string(config.subscribe_payload)));
    }

    beast::flat_buffer buffer;
    while (!check_stop()) {
      // we use a non-blocking poll or just short timeout if possible,
      // but beast sync read blocks. We can periodically check stop using async,
      // or just rely on OS to interrupt if we use signals.
      // For simplicity and performance, we do a blocking read.
      // Signals (SIGINT) will cause an EINTR or terminate the process if
      // handled asynchronously.
      buffer.consume(buffer.size());
      ws.read(buffer);

      auto now = std::chrono::system_clock::now().time_since_epoch();
      std::int64_t sec =
          std::chrono::duration_cast<std::chrono::seconds>(now).count();
      std::int64_t nsec =
          std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() %
          1000000000;

      std::string_view payload{static_cast<const char *>(buffer.data().data()),
                               buffer.size()};
      on_message(sec, nsec, payload);
    }

    ws.close(websocket::close_code::normal);
    return true;
  } catch (std::exception const &e) {
    std::cerr << "[websocket] Error: " << e.what() << "\n";
    return false;
  }
}

} // namespace bc
