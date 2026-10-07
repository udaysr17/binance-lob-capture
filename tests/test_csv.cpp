#include <cstdio>
#include <string>
#include <vector>

#include "csv_reader.h"
#include "csv_writer.h"
#include "output_file.h"
#include "test_framework.h"

TEST(csv_header_strings_exact) {
    CHECK_EQ(bc::kMarketDataHeader,
             std::string_view("recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,"
                              "conn_seq,symbol,payload_json"));
    CHECK_EQ(bc::kOrderBookHeader,
             std::string_view(
                 "tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,"
                 "bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,"
                 "ask_size2,ask_size3,ask_size4"));
    int commas = 0;
    for (char c : bc::kOrderBookHeader) commas += (c == ',');
    CHECK_EQ(commas + 1, bc::kOrderBookColumns);
}

TEST(csv_append_int) {
    std::string s;
    bc::append_int(s, -9223372036854775807LL - 1);
    s.push_back(',');
    bc::append_uint(s, 18446744073709551615ULL);
    s.push_back(',');
    bc::append_int(s, 0);
    CHECK_EQ(s, std::string("-9223372036854775808,18446744073709551615,0"));
}

TEST(csv_escape_unescape_roundtrip) {
    const std::vector<std::string> inputs = {
        "",
        "plain",
        "{\"e\":\"depthUpdate\",\"b\":[[\"1.0\",\"2\"]]}",
        "a,b,c",
        "\"",
        "\"\"",
        "quote\"in,the\"\"middle",
        "line\nbreak",
    };
    for (const auto& in : inputs) {
        std::string row = "123,";
        bc::append_csv_quoted(row, in);
        row += ",tail";
        std::string storage;
        std::vector<std::string_view> f;
        std::string err;
        CHECK(bc::parse_csv_record(row, storage, f, err));
        CHECK_EQ(f.size(), std::size_t{3});
        if (f.size() == 3) {
            CHECK_EQ(f[0], std::string_view("123"));
            CHECK_EQ(f[1], std::string_view(in));
            CHECK_EQ(f[2], std::string_view("tail"));
        }
    }
}

TEST(csv_quoting_exact_bytes) {
    std::string s;
    bc::append_csv_quoted(s, "{\"a\":\"b,c\"}");
    CHECK_EQ(s, std::string("\"{\"\"a\"\":\"\"b,c\"\"}\""));
}

TEST(csv_parse_malformed) {
    std::string storage, err;
    std::vector<std::string_view> f;
    CHECK(!bc::parse_csv_record("a,\"unterminated", storage, f, err));
    CHECK(!bc::parse_csv_record("a,\"x\"y,b", storage, f, err));
    CHECK(!bc::parse_csv_record("a,b\"c", storage, f, err));
    CHECK(bc::parse_csv_record("a,,", storage, f, err));
    CHECK_EQ(f.size(), std::size_t{3});
}

TEST(csv_reader_file_roundtrip) {
    const std::string path = "unit_test_csv_reader.tmp.csv";
    {
        bc::OutputFile out;
        std::string err;
        CHECK(out.open(path, err));
        out.buffer().append("h1,h2\n");
        out.row_done();
        out.buffer().append("1,");
        bc::append_csv_quoted(out.buffer(), "x\"y\nz,w");
        out.buffer().push_back('\n');
        out.row_done();
        out.buffer().append("2,\"crlf\"\r\n");
        out.row_done();
        out.buffer().append("3,last-no-newline");
        CHECK(out.close());
    }
    bc::CsvReader r;
    std::string err;
    CHECK(r.open(path, err));
    std::vector<std::string_view> f;
    CHECK(r.next(f, err) == bc::CsvReader::Status::Record);
    CHECK_EQ(f.size(), std::size_t{2});
    CHECK(r.next(f, err) == bc::CsvReader::Status::Record);
    CHECK_EQ(f.size(), std::size_t{2});
    if (f.size() == 2) CHECK_EQ(f[1], std::string_view("x\"y\nz,w"));
    CHECK(r.next(f, err) == bc::CsvReader::Status::Record);
    if (f.size() == 2) CHECK_EQ(f[1], std::string_view("crlf"));
    CHECK(r.next(f, err) == bc::CsvReader::Status::Record);
    if (f.size() == 2) CHECK_EQ(f[1], std::string_view("last-no-newline"));
    CHECK(r.next(f, err) == bc::CsvReader::Status::Eof);
    std::remove(path.c_str());
}
