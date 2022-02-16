#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <vector>
#include <string>

#include "util.h"
#include "potato.h"

using namespace std;

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        cout << "format:" << endl;
        cout << "player <machine_name> <port_num>" <<endl;
        cout << "(example: ./player vcm-xxxx.vm.duke.edu 1234)" << endl;
        return -1;
    }
    char const* machine_name = argv[1];
    char const* port_num = argv[2];

    int listen_fd = listen_step("0");

    int ringmaster_fd = connect_step(machine_name, port_num);

    struct sockaddr_in my_addr;
    socklen_t len = sizeof(my_addr);
    getsockname(listen_fd, (struct sockaddr *)& my_addr, &len);
    int listen_port = ntohs(my_addr.sin_port);
    send(ringmaster_fd, &listen_port, sizeof(listen_port), 0);

    int self_id, total_players;
    recv(ringmaster_fd, &self_id, sizeof(self_id), MSG_WAITALL);
    recv(ringmaster_fd, &total_players, sizeof(total_players), MSG_WAITALL);

    cout << "Connected as player " << self_id << " out of " << total_players << " total players" << endl;

    string next_ip;
    int next_port;
    int ip_size = 0;
    recv(ringmaster_fd, &ip_size, sizeof(ip_size), MSG_WAITALL);
    next_ip.resize(ip_size);

    recv(ringmaster_fd, (void*) next_ip.c_str(), ip_size, 0);
    recv(ringmaster_fd, &next_port, sizeof(next_port), MSG_WAITALL);


    int right_fd = connect_step(next_ip.c_str(), to_string(next_port).c_str());

    struct sockaddr_storage socket_addr;
    socklen_t socket_addr_len = sizeof(socket_addr);
    int left_fd = accept(listen_fd, 
        (struct sockaddr *)&socket_addr, &socket_addr_len);

    if (left_fd == -1) {
        cerr << "Error: cannot accept connection on socket" << endl;
        return -1;
    } //if

    //Begin the logic of gaming

    int nfds = 1 + max(max(left_fd, right_fd), ringmaster_fd);
    fd_set readfds;

    int fd_list[3] = {left_fd, right_fd, ringmaster_fd};

    bool game_finished = false;
    while (!game_finished) {
        for (int i = 0; i < 3; ++i) { 
            FD_SET(fd_list[i], &readfds);
        }
        select(nfds, &readfds, NULL, NULL, NULL);
        potato p;
        for (int i = 0; i < 3; ++i) {
            if (FD_ISSET(fd_list[i], &readfds)) {
                recv(fd_list[i], &p, sizeof(p), MSG_WAITALL);

                //Signal for closing fd;
                if (p.remain_hops == 0) {
                    //Signal for closing
                    for (int i = 0; i < 3; ++i) close(fd_list[0]);
                    return 0;
                }

                p.remain_hops -= 1;
                p.player_list[p.index] = self_id;
                p.index += 1;

                if (p.remain_hops == 0) {
                    //Send it back to ringmaster
                    send(ringmaster_fd, &p, sizeof(p), 0);
                    cout << "I’m it" << endl;
                } else {
                    //Send it to other players.
                    srand((unsigned int)time(NULL) + self_id);
                    int index = rand() % 2;
                    int next_hop = fd_list[index];
                    send(next_hop, &p, sizeof(p), 0);
                    int next_id = 
                        (self_id + (index ? 1: -1) + total_players) % total_players;
                    cout << "Sending potato to player " << next_id << endl;
                }
            }
        }
    }

    return 0;
}
