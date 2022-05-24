TO EXECUTE:

    In Othello-ai directory open terminal and type "$ make" command to compile the player.

    To run the player type "$ ./runall.sh" to make the player play against itself.

    Three terminal windows will open. one for each player and one showcasing the entire
    game

RUN YOUR OWN PLAYER:

    To compile another player, place the .c file in the src directory. Next open the make file and change "EXECUTABLE = player/my_player" to "EXECUTABLE = player/{name of your .c file}". In the game.json file change either paths or both paths to player/{name of your .c file}. Save all changes made to files and run as in TO EXECUTE

NOTES:

    This player was created by me using the random.c file in src_alt_player
	as a skeleton. My controbution to the player was creating the depth
	first search and minimax algoritm, imlementing alpha-beta pruning, 
	writing the utility calculation function and parralelizing all of these 
	using MPI.
	
	The comms files in the src were created by Stellenbosch University.

	Also note that this player is very inefficient against the random player 
	since the minimax algorithm assumes that the other player is also 
	attempting to play an optimal game.

	In it's curren state in the repo the player will play against itself when run.
    This is why the game state ends in the same way when run without any changes to 
    the make and game files since both players are attempting to play an optimal game.

    


