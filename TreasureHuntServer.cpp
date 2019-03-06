// William Gross
// 5/24/2016
// Game Server for Hidden Treasure game

// Purpose: A game server that creates client threads for playing a 
//			hidden treasure game. The client thread requests, receives
//			and processes guesses from a network client interface.
//			The server also maintains a leader board tracking player
//			high scores (the fewer guesses the better).

// Libraries

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#include <cmath>
#include <ctime>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// Global Constants
const short FIRST_PORT = 10900;
const short LAST_PORT = 10999;
const int MAX_PENDING = 10;
const int MAX_BUFF_LENGTH = 4096;
const int MIN = -100; const int MAX = 100;
const int MAX_TOP_SCORES = 5;

// Structs

struct leader_board_pos
{
	string name;
	int score;
};

struct thread_args
{
	int clientSock;
};

struct grid_coordinate
{
	long x;
	long y;
};

struct answer
{
	float response;
};

// Global Variables

leader_board_pos leader_board[MAX_TOP_SCORES]; // Zeroeth position is first place
int num_leaders; // the number of entires on the leaderboard (0-5)
pthread_mutex_t leader_board_lock; // Semaphore lock for leaderboard editing

// Function Signatures

void* ThreadMain(void* argp);

void ProcessClient(int clientSock);

void UpdateLeaderBoard(string name, int num_guesses);

float GetDistance(grid_coordinate guess, grid_coordinate target);

bool LeaderInsertion(leader_board_pos new_leader);

int Rand(int min, int max);


//Program Entry point
int main(int argc, char *argv[])
{
	// Variables
	int status;
	unsigned int seed = time(0); srand(seed); // RNG Initialization
	pthread_mutex_init(&leader_board_lock, NULL); // Leaderboard Lock Initialization
	

	// ********************************************************************************
	// Get/Set the necessary server connection parameters and then use them to  
	// Initialize the server socket
	//
	// Clients will be given the necessary info to connect to this socket
	// and play the game
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		cout << "Server Message: Socket error..." << endl;
		return -1;
	}

	unsigned short servPort = atoi(argv[1]);
	struct sockaddr_in servAddr;
	
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(servPort);
	
	status = bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr));
	
	if (status < 0)
	{
		cout << "Server Message: Error binding server socket..." << endl;
		return -1;
	}
	//**********************************************************************************

	// Listen for up to MAX_PENDING client connections
	status = listen(sock, MAX_PENDING);
	
	if (status < 0)
	{
		cout << "Server Message: Error setting socket to listen..." << endl;
		return -1;
	}

	// The leaderboard does not persist between server shutdowns
	for (int i = 0; i < MAX_TOP_SCORES; i++)
	{
		leader_board[i].score = -1;
	}

	num_leaders = 0;

	// Server Loop -- must be shut down via terminal
	while (true)
	{	
		// Accept client connection request
		struct sockaddr_in clientAddr;
		socklen_t addrLen = sizeof(clientAddr);
		int clientSock = accept(sock, (struct sockaddr *) &clientAddr, &addrLen);
		
		if (clientSock < 0)
		{
			cout << "Server Message: Error accepting client connection...";
		}
		else
		{
			// Create and initialize argument struct
			thread_args* ThreadArgs = new thread_args;
			ThreadArgs->clientSock = clientSock;

			// Create the client thread
			pthread_t threadID;
			status = pthread_create(&threadID, NULL, ThreadMain, (void *)ThreadArgs);
			if (status != 0)
			{
				cout << "Server Message: Error creating client thread..." << endl;
			}
		}
	}
	return 0;
}


// Function implementations

// Top level thread manager function for use in pthread_create
void* ThreadMain(void* argp)
{
	// Initializes the client socket connection
	struct thread_args *threadArgs = (struct thread_args *) argp;
	int clientSock = threadArgs->clientSock;
	delete threadArgs;
	
	// runs the game for the client connection
	ProcessClient(clientSock);

	// when the game is over, kill the thread
	pthread_detach(pthread_self());
	close(clientSock);

	return NULL;
}

// PRIMARY FUNCTION FOR REVIEW/CONSIDERATION
//
// Processes a single game-instance and network connection for a player
// Gets their name, processes their guesses, and stores/provides high
// score information.
//
void ProcessClient(int clientSock)
{
	// Variables
	int status, bytes_left, bytes_recv, bytes_sent, num_guesses; // used in server/client communication
	string player_name; // tracks number of guesses for this player, is also their score
	bool game_over = false;
	vector<char> name_buffer(MAX_BUFF_LENGTH); // used in server/client communication
	char *guess_buffer; // buffer for recieving guesses from player
	
	grid_coordinate player_guess, treasure_location;
	answer server_response;

	// Get player name
	bytes_recv = 0;
	do {
		bytes_recv = recv(clientSock, name_buffer.data(), name_buffer.size(), 0);

		if (bytes_recv == -1)
		{
			cout << "Communication error...";
			return;
		}
		else
		{
			// Build up the player's name from the recieved bytes
			player_name.append(name_buffer.begin(), name_buffer.end());
		}
	
	} while (bytes_recv == MAX_BUFF_LENGTH);

	// Prepare game
	treasure_location.x = Rand(MIN, MAX);
	treasure_location.y = Rand(MIN, MAX);
	num_guesses = 0;
	
	// Game loop
	while (!game_over)
	{
		num_guesses++;

		// Receive loop; gets player guess
		
		// ***************************************************************************************************
		// Get x coordinate

		// Initialize data transfer vars
		bytes_left = sizeof(long);
		guess_buffer = (char *)&player_guess.x; // the thread-buffer points at the player object's x-guess

		// read data from client connection
		while (bytes_left)
		{
			// read data from client connection
			bytes_recv = recv(clientSock, guess_buffer, bytes_left, 0);
			if (bytes_recv <= 0)
			{
				cout << "Server Message: Communication error..." << endl;
				return;
			}
			// ensure the correct number of bytes is received
			bytes_left -= bytes_recv;

			guess_buffer += bytes_recv; // read the bytes into the guess buffer which -> player_guess.y
		}
		// Convert guess from network-byte-order to host-byte-order
		player_guess.x = ntohl(player_guess.x);
		player_guess.x -= 100;
		
		// ***************************************************************************************************
		// Get y coordinate

		// Initialize data transfer vars
		bytes_left = sizeof(long);
		guess_buffer = (char *)&player_guess.y; // the thread-buffer points at the player object's y-guess

		// read data from client connection
		while (bytes_left)
		{
			bytes_recv = recv(clientSock, guess_buffer, bytes_left, 0);
			if (bytes_recv <= 0)
			{
				cout << "Server Message: Communication error..." << endl;
				return;
			}
			// ensure the correct number of bytes is received
			bytes_left -= bytes_recv;

			guess_buffer += bytes_recv; // read the bytes into the guess buffer which -> player_guess.y
		}	
		// Convert guess from network-byte-order to host-byte-order
		player_guess.y = ntohl(player_guess.y);
		player_guess.y -= 100;		
	

		// PROCESS GUESS

		// The server simply responds with how far the player was from the mark
		server_response.response = GetDistance(player_guess, treasure_location);
		
		// If the player guessed correctly, check/insert them into the leaderboard
		if (server_response.response == 0)
		{
			pthread_mutex_lock(&leader_board_lock);
			UpdateLeaderBoard(player_name, num_guesses);
			pthread_mutex_unlock(&leader_board_lock);
			
			game_over = true;
		}
		
		// Send response
		bytes_sent = send(clientSock, (void *)&server_response.response, sizeof(float), 0);

		if (bytes_sent != sizeof(float))
		{
			cout << "Server Message: Communication error..." << endl;
			return;
		}
	}
	
	// Package and send the current leaderboard to the client 
	int i = 0;
	string leader_board_package; string append_buffer;
	string dubspace = "  ";
	ostringstream oss;
	
	// Format the leaderboard so the client-code can easily display it
	leader_board_package = "Leader Board: \n";
	while(i < num_leaders)
	{
		oss << leader_board[i].name << dubspace << leader_board[i].score << endl;
		leader_board_package += oss.str();
		oss.str(string());
		i++;
	}
	
	bytes_sent = send(clientSock, leader_board_package.c_str(), leader_board_package.length(), 0);
	
	if (bytes_sent != leader_board_package.length())
	{
		cout << "Server Message: Communication error..." << endl;
		return;
	}
	leader_board_package.clear();
	
	return;
}

// Adds a new leader to the leader boards
void UpdateLeaderBoard(string name, int num_guesses)
{
	bool done = false;
	leader_board_pos new_leader;

	new_leader.name = name;
	new_leader.score = num_guesses;

	// If leaderboard is empty
	if (num_leaders == 0)
	{
		leader_board[0] = new_leader;
		
		num_leaders++;
	}
	else
	{
		if (LeaderInsertion(new_leader))
		{
			if(num_leaders < MAX_TOP_SCORES)
			{
				num_leaders++;
			}
		}
	}
}

// Calculates how for off a user's guess was from the target on a 2x2 grid
float GetDistance(grid_coordinate guess, grid_coordinate target)
{
	int temp_x = (target.x - guess.x);
	int temp_y = (target.y - guess.y);
	
	float distance = sqrt((double) temp_x * temp_x + temp_y * temp_y);
	return distance;
}

// Inserts new_leader into the high score board at the appropriate position
bool LeaderInsertion(leader_board_pos new_leader)
{
	int i = 0;
	int j;
	bool done = false;
	bool insert = false;
	
	while (!done)
	{
		// Check if the score from this game is a new high score by 
		// comparing it to all existing scores in descending order
		//
		// NOTE: if the number of saved high scores == MAX_TOP_SCORES
		// the lowest high score will get dropped if new_leader
		// gets added
		if (new_leader.score >= leader_board[i].score)
		{
			if (i < MAX_TOP_SCORES) // if the new score is greater than the current lowest score
			{                       // insert it and shift all lowers positions accordingly
				for (int j = (MAX_TOP_SCORES - 1); j >= i; j--)
				{
					if(j > 0)
					{
						leader_board[j] = leader_board[j - 1];
					}
				}
				leader_board[i] = new_leader;
				insert = true;
				done = true;
			}
		}
		i++;
		if (i == MAX_TOP_SCORES)
		{
			done = true;
		}
	}
	return insert;
}

// Basic random number generator function
int Rand(int min, int max)
{
	return min + (rand() % max - min + 1);
}