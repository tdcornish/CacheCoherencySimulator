#include "MESI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MESI_protocol::MESI_protocol(Hash_table *my_table, Hash_entry *my_entry) :
		Protocol(my_table, my_entry) {

	this->state = MESI_CACHE_I;
}

MESI_protocol::~MESI_protocol() {
}

void MESI_protocol::dump(void) {
	const char *block_states[5] = { "X", "I", "S", "E", "M" };
	fprintf(stderr, "MESI_protocol - state: %s\n", block_states[state]);
}

void MESI_protocol::process_cache_request(Mreq *request) {
	switch (state) {
	case MESI_CACHE_I:
		do_cache_I(request);
		break;
	case MESI_CACHE_E:
		do_cache_E(request);
		break;
	case MESI_CACHE_S:
		do_cache_S(request);
		break;
	case MESI_CACHE_M:
		do_cache_M(request);
		break;
	default:
		fatal_error("Invalid Cache State for MESI Protocol\n");
	}
}

void MESI_protocol::process_snoop_request(Mreq *request) {
	switch (state) {
	case MESI_CACHE_I:
		do_snoop_I(request);
		break;
	case MESI_CACHE_E:
		do_snoop_E(request);
		break;
	case MESI_CACHE_S:
		do_snoop_S(request);
		break;
	case MESI_CACHE_M:
		do_snoop_M(request);
		break;
	case MESI_CACHE_IM:
		do_snoop_IM(request);
		break;
	case MESI_CACHE_ISE:
		do_snoop_ISE(request);
		break;
	default:
		fatal_error("Invalid Cache State for MESI Protocol\n");
	}
}

inline void MESI_protocol::do_cache_I(Mreq *request) {
	switch (request->msg) {
	case LOAD:
		//Request data with no intent to modify. Move to state ISE to await DATA message
		send_GETS(request->addr);
		state = MESI_CACHE_ISE;

		//Cache miss as well
		Sim->cache_misses++;
		break;
	case STORE:
		//Request data with intent to modify. Move to state IM to await DATA message
		send_GETM(request->addr);
		state = MESI_CACHE_IM;

		//Cache miss as well
		Sim->cache_misses++;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_cache_S(Mreq *request) {
    switch(request->msg){
        case LOAD:
            //Cache hit so send data to processor but nothing else happens.
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            //Upgrade to M state, send GETM to get most up to date value and transition to IM state.
            send_GETM(request->addr);
            state = MESI_CACHE_IM;

            //Counts as a cache miss
            Sim->cache_misses++;
            break;
        default:
            request->print_msg(my_table->moduleID, "ERROR");
            fatal_error("Client: S state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_cache_E(Mreq *request) {
    switch(request->msg){
        case LOAD:
            //Cache hit, send data to processor
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            //Want to modify the value and have the only copy of the data so silently upgrade M state.
            state = MESI_CACHE_M;
            Sim->silent_upgrades++;
            break;
        default:
            request->print_msg(my_table->moduleID, "ERROR");
            fatal_error("Client: E state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_cache_M(Mreq *request) {
    switch(request->msg){
        case LOAD:
        case STORE:
            //cache hit, send data to processor
            send_DATA_to_proc(request->addr);
            break;
        default:
            request->print_msg(my_table->moduleID, "ERROR");
            fatal_error("Client: M state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_snoop_I(Mreq *request) {

}

inline void MESI_protocol::do_snoop_S(Mreq *request) {

}

inline void MESI_protocol::do_snoop_E(Mreq *request) {

}

inline void MESI_protocol::do_snoop_M(Mreq *request) {

}

inline void MESI_protocol::do_snoop_IM(Mreq *request) {
	switch (request->msg) {
	case GETM:
	case GETS:
		//Do nothing, doesn't affect this state.
		break;
	case DATA:
		//Got the data we requested, send it to processor and move to M state.
		send_DATA_to_proc(request->addr);
		state = MESI_CACHE_M;
		break;
	default:
		break;
	}
}
inline void MESI_protocol::do_snoop_ISE(Mreq *request) {
	switch (request->msg) {
	case GETM:
	case GETS:
		//Do nothing, doesn't affect this state.
		break;
	case DATA:
		//Got the data we requested, send it to processor and move to S state if the shared line is set, E if the shared line is not set.
		send_DATA_to_proc(request->addr);
		if (get_shared_line()) {
			state = MESI_CACHE_S;
		} else {
			state = MESI_CACHE_E;
		}
		break;
	default:
		break;
	}
}
