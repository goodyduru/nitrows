#include "events.h"
#include "net.h"
#include "server.h"


int main(){
    int listener_socket = get_listener_socket();
    init_event_loop();
    add_to_event_loop(listener_socket);
    run_event_loop(listener_socket, accept_connection, handle_connection);
}