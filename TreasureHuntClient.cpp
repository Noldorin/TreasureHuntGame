// William Gross
// 5/24/2016
// Game Client for Hidden Treasure game

// Purpose: A game client that connects to a server in order
//			to play a hidden treasure game. The client connects
//			to the server using the provided cmd line args, 
//			then sends guesses and receives responses from the 
//			server until the game is completed. The player's
//			score is saved on the server's leader board if it's
//			high enough.

// Libraries

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// Global Constants
const int MAX_BUFF_LENGTH = 4096;

// Structs

struct grid_coordinate
{
	long x;
	long y;
};

struct guess_to_target
{
	float response;
};

// Global Variables

// Function Signatures

int main(int argc, char *argv[])
{
	// General Variables
	int status, bytes_recv, bytes_sent;
	bool game_over;
	char *IPAddr = argv[1];
	vector<char> leader_board_buffer(MAX_BUFF_LENGTH);
	unsigned short servPort = atoi(argv[2]);
	unsigned long servIP;

	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		cout << "Socket error..." << endl;
		return 0;
	}

	// Connect to server
	// **************************************************
	status = inet_pton(AF_INET, IPAddr, (void *)&servIP);
	if (status <= 0)
	{
		cout << "IP conversion error..." << endl;
		return 0;
	}

	sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = servIP;
	servAddr.sin_port = htons(servPort);

	status = connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr));
	if (sock < 0)
	{
		cout << "Error connecting to server..." << endl;
		return 0;
	}
	// *************************************************


	// Additional variables and structures needed for game
	guess_to_target ServerAns; // Holds guess-response from server
	grid_coordinate UserGuess; // Holds the users guess
	char *response_buffer = (char *)&ServerAns.response; // used in Client/server communication
	string name; int turn = 0;
	string leader_board_package; // used in Client/server communication
	int bytes_left; // used in Client/server communication
	ServerAns.response = -1;

	// Get players name and send it to server
	cout << "Welcome to treasure hunt! Please enter your name: ";
	getline(cin, name);

	if ((send(sock, name.c_str(), name.length(), 0)) == -1)
	{
		cout << "Communication Error... >";
		return 0;
	}


	bytes_left = 0; game_over = false;

	// Game loop
	while (!game_over)
	{
		// Receive loop
		while (bytes_left)
		{
			bytes_recv = recv(sock, response_buffer, bytes_left, 0);
			if (bytes_recv <= 0)
			{
				cout << "Connection error... >";
				return 0;
			}

			bytes_left -= bytes_recv;
			response_buffer += bytes_recv;
		}

		// Process response and display to user
		if (ServerAns.response > 0)
		{
			cout << endl << "Distance to treasure: " << ServerAns.response
				<< "ft" << endl << endl;
		}
		else if (ServerAns.response == 0)
		{
			cout << endl << "You found the treasure!" << endl
				<< "It took " << turn << " turns to find the treasure.";
			cin.ignore();
			game_over = true;
		}

		if (!game_over)
		{
			turn++;
			cout << "Turn: " << turn;

			// Get guess
			cout << endl << "Enter the x coordinate of your guess: ";
			cin >> UserGuess.x; cin.ignore();
			UserGuess.x += 100;
			UserGuess.x = htonl(UserGuess.x);
			cout << "Enter the y coordinate of your guess: ";
			cin >> UserGuess.y; cin.ignore();
			UserGuess.y += 100;
			UserGuess.y = htonl(UserGuess.y);

			// Send Guess

			// Send x
			bytes_sent = send(sock, (void *)&UserGuess.x, sizeof(long), 0);
			if (bytes_sent != sizeof(long))
			{
				cout << "Connection error... >";
				cin.ignore();
				return 0;
			}

			// Send y
			bytes_sent = send(sock, (void *)&UserGuess.y, sizeof(long), 0);
			if (bytes_sent != sizeof(long))
			{
				cout << "Connection error... >";
				cin.ignore();
				return 0;
			}

			bytes_left = sizeof(float);
			ServerAns.response = -1;
			response_buffer = (char *)&ServerAns.response;
		}
	}

	//Get and display Leaderboard
	bytes_recv = 0;
	do {

		bytes_recv = recv(sock, leader_board_buffer.data(), MAX_BUFF_LENGTH, 0);

		if (bytes_recv == -1)
		{
			cout << "Communication error...";
			return 0;
		}
		else
		{
			leader_board_package.append(leader_board_buffer.begin(), leader_board_buffer.end());
		}

	} while (bytes_recv == MAX_BUFF_LENGTH);

	cout << endl << endl << leader_board_package << endl << "> "; cin.ignore();
	cout << endl << "Thanks for playing!" << endl;

	return 0;
}