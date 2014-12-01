#include "MSI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MSI_protocol::MSI_protocol(Hash_table *my_table, Hash_entry *my_entry) :
		Protocol(my_table, my_entry) {
	//Init lines to be invalid.
	this->state = MSI_CACHE_I;
}

MSI_protocol::~MSI_protocol() {
}

void MSI_protocol::dump(void) {
	const char *block_states[4] = { "X", "I", "S", "M" };
	fprintf(stderr, "MSI_protocol - state: %s\n", block_states[state]);
}

void MSI_protocol::process_cache_request(Mreq *request) {
	switch (state) {
	case MSI_CACHE_I:
		do_cache_I(request);
		break;
	case MSI_CACHE_S:
		do_cache_S(request);
		break;
	case MSI_CACHE_M:
		do_cache_M(request);
		break;
	default:
		fatal_error("Invalid Cache State for MSI Protocol\n");
	}
}

void MSI_protocol::process_snoop_request(Mreq *request) {
	switch (state) {
	case MSI_CACHE_I:
		do_snoop_I(request);
		break;
	case MSI_CACHE_S:
		do_snoop_S(request);
		break;
	case MSI_CACHE_M:
		do_snoop_M(request);
		break;
	case MSI_CACHE_IM:
		do_snoop_IM(request);
		break;
	case MSI_CACHE_IS:
		do_snoop_IS(request);
		break;
	default:
		fatal_error("Invalid Cache State for MSI Protocol\n");
	}
}

inline void MSI_protocol::do_cache_I(Mreq *request) {
	switch (request->msg) {
	case LOAD:
		//Request the data with no intention to modify it, so send a GETS on the bus and transition to the S state.
		send_GETS(request->addr);
		state = MSI_CACHE_IS;

		//also a cache miss
		Sim->cache_misses++;
		break;
	case STORE:
		//Request the data with intention to modify it, so send a GETM on the bus and transition to the M state.
		send_GETM(request->addr);
		state = MSI_CACHE_IM;

		//also a cache miss
		Sim->cache_misses++;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: I state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_cache_S(Mreq *request) {
	switch (request->msg) {
	case LOAD:
		//Cache hit so send data back to processor.
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		//Modified the data, need to issue a GETM and transition to the M state
		send_GETM(request->addr);
		state = MSI_CACHE_IM;

		//Counts as a cache miss
		Sim->cache_misses++;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: S state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_cache_M(Mreq *request) {
	switch (request->msg) {
	case LOAD:
	case STORE:
		//Cache hit, so send data back to processor.
		send_DATA_to_proc(request->addr);
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: M state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_snoop_I(Mreq *request) {
	//Do nothing
	return;
}

inline void MSI_protocol::do_snoop_IS(Mreq *request) {
	switch (request->msg) {
	case GETM:
	case GETS:
		//Nothing, my request has already been made and must be completed.
		break;
	case DATA:
		//Got the DATA we requested, send to processor and move to S state.
		send_DATA_to_proc(request->addr);
		state = MSI_CACHE_S;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: To M state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_snoop_S(Mreq *request) {
	switch (request->msg) {
	case GETM:
		//someone else has/is going to modify this data, move to I state.
		state = MSI_CACHE_I;
		break;
	case GETS:
		//do nothing, doesn't affect state.
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: M state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_snoop_IM(Mreq *request) {
	switch (request->msg) {
	case GETM:
	case GETS:
		//Nothing, someone else's GETS and GETM.
		break;
	case DATA:
		//Got the DATA we requested, send to processor and move to M state.
		send_DATA_to_proc(request->addr);
		state = MSI_CACHE_M;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: To M state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_snoop_M(Mreq *request) {
	switch (request->msg) {
	case GETM:
		//someone else is requesting my current copy of the data to modify. Send it on the bus and move to State I.
		send_DATA_on_bus(request->addr, request->src_mid);
		state = MSI_CACHE_I;
		break;
	case GETS:
		//Someone is requesting my data but not to modify. Send it on the bus and move to State S.
		send_DATA_on_bus(request->addr, request->src_mid);
		state = MSI_CACHE_S;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: M state shouldn't see this message\n");
	}
}
