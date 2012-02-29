#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <list>
#include <iostream>

#include "netcalc.h"

#define DEFAULT_HOST	"localhost"
#define HOST_PARAM		"--host"

int sock;
struct addrinfo *server;

std::list<nc_value_t> stack;

// Executes a single query on the remote server.
nc_value_t executeQuery( nc_query_t &query ) {
	// Send query.
	assert( sendto( sock, &query, sizeof( query ), 0, server->ai_addr, server->ai_addrlen ) == sizeof( query ) );
	// Get response.
	nc_response_t response;
	assert( recv( sock, &response, sizeof( response ), 0 ) == sizeof( response ) );
	// Output operation.
	if ( response.err == NC_OK ) {
		std::cerr << "	" << query.arg1 << " " << getOpName( query.op ) << " " << query.arg2 << " = " << response.value << std::endl;
	} else {
		std::cerr << "Error: " << getErrText( response.err ) << std::endl;
	}
	assert( response.err == NC_OK && "Query failed." );

	return response.value;
}

// Handle an RPN operation (either an operator or a number to push).
void handleRPNOp( std::string rpnOp ) {
	nc_operator_t ncOp;

	// Check whether operator or number.
	if ( (ncOp = getOpByName( rpnOp )) == NC_OPERATOR_END ) { // Not a known operator. Assume number.
		stack.push_back( atoi( rpnOp.c_str() ) );
	} else {
		assert( stack.size() >= 2 && "Not enough data in stack to run an operation." );
		// Pop arguments.
		nc_query_t query;
		query.op = ncOp;
		query.arg2 = stack.back();
		stack.pop_back();
		query.arg1 = stack.back();
		stack.pop_back();
		// Push result.
		stack.push_back( executeQuery( query ) );
	}
}

// Output stack elements.
void dumpStack() {
	for ( std::list<nc_value_t>::iterator it = stack.begin(), ie = stack.end(); it != ie; it++ ) {
		if ( it != stack.begin() )
			std::cout << "	";
		std::cout << *it;
	}
	std::cout << std::endl;
}

// Execute a list of RPN operations and output result.
void executeRPN( std::list<std::string> rpn ) {
	for ( std::list<std::string>::iterator it = rpn.begin(), ie = rpn.end(); it != ie; it++ )
		handleRPNOp( *it );
	dumpStack();
}

int main( int argc, char **argv ) {
	std::list<std::string> args( argv, argv + argc );

	// Remove program name.
	args.pop_front();

	const char *host = DEFAULT_HOST;
	// Check if user specified a host.
	if ( args.size() > 2 && args.front() == HOST_PARAM ) {
		args.pop_front();
		host = args.front().c_str();
		args.pop_front();
	}
	std::cerr << "Sending queries to " << host << "." << std::endl;

	struct addrinfo hints, *result;
	bzero( &hints, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	assert( ! getaddrinfo( host, NULL, &hints, &result ) );

	for ( server = result; server; server = server->ai_next ) {
		((struct sockaddr_in *) result->ai_addr)->sin_port = htons( NETCALC_UDP_PORT );
		if ( (sock = socket( server->ai_family, server->ai_socktype, server->ai_protocol )) < 0 )
			continue;
		break;
	}
	assert( sock >= 0 && "Host could not be resolved." );

	executeRPN( args );

	freeaddrinfo( result );
}
