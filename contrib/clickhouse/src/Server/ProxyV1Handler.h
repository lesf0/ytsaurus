#pragma once

#include <DBPoco/Net/TCPServerConnection.h>
#include <Server/IServer.h>
#include <Server/TCPProtocolStackData.h>


namespace DB
{

class ProxyV1Handler : public DBPoco::Net::TCPServerConnection
{
    using StreamSocket = DBPoco::Net::StreamSocket;
public:
    explicit ProxyV1Handler(const StreamSocket & socket, IServer & server_, const std::string & conf_name_, TCPProtocolStackData & stack_data_)
        : DBPoco::Net::TCPServerConnection(socket), log(getLogger("ProxyV1Handler")), server(server_), conf_name(conf_name_), stack_data(stack_data_) {}

    void run() override;

protected:
    bool readWord(int max_len, std::string & word, bool & eol);

private:
    LoggerPtr log;
    IServer & server;
    std::string conf_name;
    TCPProtocolStackData & stack_data;
};

}
