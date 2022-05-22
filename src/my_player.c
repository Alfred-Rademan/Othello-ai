/* vim: :se ai :se sw=4 :se ts=4 :se sts :se et */

/*H**********************************************************************
 *
 *    This is a skeleton to guide development of Othello engines that can be used
 *    with the Ingenious Framework and a Tournament Engine. 
 *
 *    The communication with the referee is handled by an implementaiton of comms.h,
 *    All communication is performed at rank 0.
 *
 *    Board co-ordinates for moves start at the top left corner of the board i.e.
 *    if your engine wishes to place a piece at the top left corner, 
 *    the "gen_move_master" function must return "00".
 *
 *    The match is played by making alternating calls to each engine's 
 *    "gen_move_master" and "apply_opp_move" functions. 
 *    The progression of a match is as follows:
 *        1. Call gen_move_master for black player
 *        2. Call apply_opp_move for white player, providing the black player's move
 *        3. Call gen move for white player
 *        4. Call apply_opp_move for black player, providing the white player's move
 *        .
 *        .
 *        .
 *        N. A player makes the final move and "game_over" is called for both players
 *    
 *    IMPORTANT NOTE:
 *        Write any (debugging) output you would like to see to a file. 
 *        	- This can be done using file fp, and fprintf()
 *        	- Don't forget to flush the stream
 *        	- Write a method to make this easier
 *        In a multiprocessor version 
 *        	- each process should write debug info to its own file 
 *H***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <time.h>
#include <assert.h>
#include "comms.h"

const int EMPTY = 0;
const int BLACK = 1;
const int WHITE = 2;

const int OUTER = 3;
const int ALLDIRECTIONS[8] = {-11, -10, -9, -1, 1, 9, 10, 11};
const int BOARDSIZE = 100;

const int PRUNE = -200;
const int DEPTH = 7;
const int LEGALMOVSBUFSIZE = 65;
const char piecenames[4] = {'.','b','w','?'};

void run_master(int argc, char *argv[]);
int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp);
void gen_move_master(char *move, int my_colour, FILE *fp);
void apply_opp_move(char *move, int my_colour, FILE *fp);
void game_over();
void run_worker();
void initialise_board();
void free_board();

int calc_util (int *curr_board,int curr_player,
int curr_bcount, int curr_wcount, int curr_depth);

int find_maxmin(int maxlayer, int *util, int amount_moves);

int minmax(int *curr_board, int *moves, int depth, int curr_layer,
int curr_player,int curr_bcount, int curr_wcount, int alpha,
int beta, int maxmin, int first_player, FILE *fp);
void legal_moves(int *curr_board, int player, int *moves, FILE *fp);
int legalp(int *curr_board, int move, int player, FILE *fp);
int validp(int move);
int would_flip(int *curr_board, int move, int dir, int player, FILE *fp);
int opponent(int player, FILE *fp);
int find_bracket_piece(int *curr_board, int square, int dir, int player, FILE *fp);
int random_strategy(int *curr_board, int my_colour, FILE *fp);
void make_move(int *curr_board, int move, int player, FILE *fp);
void make_flips(int *curr_board, int move, int dir, int player, FILE *fp);
int get_loc(char* movestring);
void get_move_string(int loc, char *ms);
void print_board(FILE *fp);
char nameof(int piece);
int count(int *curr_board, int player);
FILE *dump = NULL;
int *board;
int alpha = -1000;
int beta = 1000;


int main(int argc, char *argv[]) {
	int rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
 
	initialise_board(); //one for each process

	if (rank == 0) {
	    run_master(argc, argv);
	} else {
	    run_worker(rank);
	}
	game_over();

}

void run_master(int argc, char *argv[]) {
	char cmd[CMDBUFSIZE];
	char my_move[MOVEBUFSIZE];
	char opponent_move[MOVEBUFSIZE];
	int time_limit;
	int my_colour;
	int running = 0;
	FILE *fp = NULL;
	if (initialise_master(argc, argv, &time_limit, &my_colour, &fp) != FAILURE) {
		running = 1;
	}
	if (my_colour == EMPTY) my_colour = BLACK;
	while (running == 1) {
		/* Receive next command from referee */
		if (comms_get_cmd(cmd, opponent_move) == FAILURE) {
			fprintf(fp, "Error getting cmd\n");
			fflush(fp);
			running = 0;
			break;
		}

		/* Received game_over message */
		if (strcmp(cmd, "game_over") == 0) {
			running = 0;
			fprintf(fp, "Game over\n");
			fflush(fp);
			break;

		/* Received gen_move message */
		} else if (strcmp(cmd, "gen_move") == 0) {
			MPI_Bcast(&running,1,MPI_INT,0,MPI_COMM_WORLD);
			fflush(stdout);
			gen_move_master(my_move, my_colour, fp);
			fflush(stdout);
			print_board(fp);

			if (comms_send_move(my_move) == FAILURE) { 
				running = 0;
				fprintf(fp, "Move send failed\n");
				fflush(fp);
				break;
			}

		/* Received opponent's move (play_move mesage) */
		} else if (strcmp(cmd, "play_move") == 0) {
			apply_opp_move(opponent_move, my_colour, fp);
			print_board(fp);

		/* Received unknown message */
		} else {
			fprintf(fp, "Received unknown command from referee\n");
		}
	}
	MPI_Bcast(&running,1,MPI_INT,0,MPI_COMM_WORLD);
}

int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp) {
	int result = FAILURE;

	if (argc == 5) { 
		unsigned long ip = inet_addr(argv[1]);
		int port = atoi(argv[2]);
		*time_limit = atoi(argv[3]);

		*fp = fopen(argv[4], "w");
		if (*fp != NULL) {
			fprintf(*fp, "Initialise communication and get player colour \n");
			if (comms_init_network(my_colour, ip, port) != FAILURE) {
				result = SUCCESS;
			}
			fflush(*fp);
		} else {
			fprintf(stderr, "File %s could not be opened", argv[4]);
		}
	} else {
		fprintf(*fp, "Arguments: <ip> <port> <time_limit> <filename> \n");
	}
	
	return result;
}

void initialise_board() {
	int i;
	board = (int *) malloc(BOARDSIZE * sizeof(int));
	for (i = 0; i <= 9; i++) board[i] = OUTER;
	for (i = 10; i <= 89; i++) {
		if (i%10 >= 1 && i%10 <= 8) board[i] = EMPTY; else board[i] = OUTER;
	}
	for (i = 90; i <= 99; i++) board[i] = OUTER;
	board[44] = WHITE; board[45] = BLACK; board[54] = BLACK; board[55] = WHITE;
}

void free_board() {
	free(board);
}

/**
 *   Rank i (i != 0) executes this code 
 *   ----------------------------------
 *   Called at the start of execution on all ranks except for rank 0.
 *   - run_worker should play minimax from its move(s) 
 *   - results should be send to Rank 0 for final selection of a move 
 */
void run_worker() {
	int ranks;
	int *curr_colour = (int *) malloc(sizeof(int));
	int *move_amount = (int *) malloc(sizeof(int));
	int *curr_board = (int *) malloc(BOARDSIZE * sizeof(int));
	int running = 1;
	int *curr_bcount = (int *)malloc(sizeof(int));
	int *curr_wcount = (int *)malloc(sizeof(int));
	int ret_vals[2] = {0,0};
	MPI_Comm_size(MPI_COMM_WORLD,&ranks);	
	MPI_Bcast(&running,1,MPI_INT,0,MPI_COMM_WORLD);

	while (running == 1) {
		MPI_Bcast(curr_bcount,1,MPI_INT,0,MPI_COMM_WORLD);
		MPI_Bcast(curr_wcount,1,MPI_INT,0,MPI_COMM_WORLD);
		MPI_Bcast(curr_colour,1,MPI_INT,0,MPI_COMM_WORLD);
		MPI_Bcast(curr_board,1,MPI_INT,0,MPI_COMM_WORLD);
		MPI_Bcast(move_amount,1,MPI_INT,0,MPI_COMM_WORLD);
		int *my_moves = (int *)malloc(*move_amount/ranks * sizeof(int));
		MPI_Scatterv(NULL, NULL, NULL, MPI_INT,my_moves, *move_amount/ranks ,
		MPI_INT,0, MPI_COMM_WORLD);
		int *feed = (int*) malloc(LEGALMOVSBUFSIZE * sizeof(int));
		feed[0] = *move_amount/ranks;
		for (int i = 1; i <= feed[0]; i++){
			feed[i] = my_moves[i-1];
		}

		int util;
		for (int i = 0; i < feed[0]; i++){
			 util = minmax(curr_board,feed,DEPTH,0,*curr_colour,*curr_bcount,*curr_wcount
			,alpha,beta,1,*curr_colour,dump);
			if(i == 0 || util > ret_vals[0]){
				ret_vals[0] = util;
				ret_vals[1] = feed[i+1];
			}
		}
		free(feed);
		free(my_moves);
		MPI_Gatherv(ret_vals, 2, MPI_INT, NULL, NULL, NULL, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&running,1,MPI_INT,0,MPI_COMM_WORLD);
	}
	free(curr_colour);
	free(move_amount);
	free(curr_bcount);
	free(curr_wcount);
}

/**
 *  Rank 0 executes this code: 
 *  --------------------------
 *  Called when the next move should be generated 
 *  - gen_move_master should play minimax from its move(s)
 *  - the ranks may communicate during execution 
 *  - final results should be gathered at rank 0 for final selection of a move 
 */
void gen_move_master(char *move, int my_colour, FILE *fp) {
	int loc;
	int ranks;
	int thread_move;
	int *run = (int *) malloc(sizeof(int));
	*run = 1;
	int *curr_colour = (int *) malloc(sizeof(int));
	*curr_colour = my_colour;
	int *curr_board = board;
	int *move_amount = (int *)malloc (sizeof(int));
	int *curr_bcount = (int *)malloc(sizeof(int));
	* curr_bcount = count(board,BLACK);
	int *curr_wcount = (int *)malloc(sizeof(int));
	* curr_wcount = count(board,WHITE);
	MPI_Comm_size(MPI_COMM_WORLD,&ranks);
	int disp[ranks];
	int g_disp[ranks];
	int send_count[ranks];
	int g_count[ranks];
	int gatherer[ranks*2];
	int ret_vals[2] = {0,0};

	/* generate move */
	MPI_Bcast(curr_bcount,1,MPI_INT,0,MPI_COMM_WORLD);
	MPI_Bcast(curr_wcount,1,MPI_INT,0,MPI_COMM_WORLD);
	MPI_Bcast(curr_colour,1,MPI_INT,0,MPI_COMM_WORLD);
	MPI_Bcast(curr_board,1,MPI_INT,0,MPI_COMM_WORLD);
	int *moves = (int *) malloc(LEGALMOVSBUFSIZE * sizeof(int));
	memset(moves, 0, LEGALMOVSBUFSIZE);
	legal_moves(board, my_colour, moves, dump);

	*move_amount = moves[0];

	int *my_moves = (int *) malloc(((*move_amount/ranks)+
	 (*move_amount%ranks))*sizeof(int));
	thread_move = (*move_amount/ranks)+ (*move_amount%ranks);
	MPI_Bcast(move_amount,1,MPI_INT,0,MPI_COMM_WORLD);
	for (int i = 0; i <ranks ; i++) {
		if (i == 0) {
			send_count[i] = thread_move;
			disp[i] = 1;  
			
		} else {
			send_count[i] = *move_amount/ranks; 
			disp[i] = (*move_amount/ranks)*i + *move_amount%ranks +1;
		}
		g_count[i] = 2;
		g_disp[i] = 2*i;
	}

	/*Distribute all possible moves from original board state to simulated
	 *  boards in other threads;
	 */
	MPI_Scatterv(moves, send_count, disp, MPI_INT,my_moves, thread_move,
	MPI_INT,0, MPI_COMM_WORLD);

	int *feed = (int*) malloc(LEGALMOVSBUFSIZE * sizeof(int));
	feed[0] = thread_move;
	for (int i = 1; i <= feed[0]; i++){
		feed[i] = my_moves[i-1];
	}
	int util;
	for (int i = 0; i < feed[0]; i++){
		 util = minmax(curr_board,feed,DEPTH,0,*curr_colour,*curr_bcount,*curr_wcount
		,alpha,beta,0,*curr_colour,dump);
		if(i == 0 || util > ret_vals[0]){
			ret_vals[0] = util;
			ret_vals[1] = feed[i+1];
		}
	}
	free(feed);

	/*Returns utility maximazing moves and utility values from treads */
	MPI_Gatherv(ret_vals, 2, MPI_INT, gatherer, g_count, g_disp, MPI_INT,0, MPI_COMM_WORLD);
	int mover = gatherer[1];
	util = gatherer[0];
	for (int i = 0; i < ranks*2; i = i + 2){	
		
		if (util > gatherer[i]) {
			util = gatherer[i];
			mover = gatherer[i+1];
		}
	}
	if( 11<=mover && mover<=88){
		loc = mover;
	} else {
		loc = -1;
	}
	*run = 0;
	if (loc == -1) {
		strncpy(move, "pass\n", MOVEBUFSIZE);
	} else {
		/* apply move */
		get_move_string(loc, move);
		make_move(board, loc, my_colour, fp);
	}
	free(moves);
	free(curr_colour);
	free(move_amount);
	free(curr_bcount);
	free(curr_wcount);
}

void apply_opp_move(char *move, int my_colour, FILE *fp) {
	int loc;
	if (strcmp(move, "pass\n") == 0) {
		return;
	}
	loc = get_loc(move);
	make_move(board, loc, opponent(my_colour, fp), fp);
}

void game_over() {
	free_board();
	MPI_Finalize();
}

void get_move_string(int loc, char *ms) {
	int row, col, new_loc;
	new_loc = loc - (9 + 2 * (loc / 10));
	row = new_loc / 8;
	col = new_loc % 8;
	ms[0] = row + '0';
	ms[1] = col + '0';
	ms[2] = '\n';
	ms[3] = 0;
}

int get_loc(char* movestring) {
	int row, col;
	/* movestring of form "xy", x = row and y = column */ 
	row = movestring[0] - '0'; 
	col = movestring[1] - '0'; 
	return (10 * (row + 1)) + col + 1;
}

void legal_moves(int *curr_board,int player, int *moves, FILE *fp) {
	int move, i;
	moves[0] = 0;
	i = 0;
	for (move = 11; move <= 88; move++)
		if (legalp(curr_board, move, player, fp)) {
		i++;
		moves[i] = move;
	}
	moves[0] = i;
}

int legalp(int *curr_board, int move, int player, FILE *fp) {
	int i;
	if (!validp(move)) return 0;
	if (curr_board[move] == EMPTY) {
		i = 0;
		while (i <= 7 && !would_flip(curr_board, move, ALLDIRECTIONS[i], player, fp)) i++;
		if (i == 8) return 0; else return 1;
	}
	else return 0;
}

int validp(int move) {
	if ((move >= 11) && (move <= 88) && (move%10 >= 1) && (move%10 <= 8))
		return 1;
	else return 0;
}

int would_flip(int *curr_board, int move, int dir, int player, FILE *fp) {
	int c;
	c = move + dir;
	if (curr_board[c] == opponent(player, fp))
		return find_bracket_piece(curr_board, c+dir, dir, player, fp);
	else return 0;
}

int find_bracket_piece(int *curr_board, int square, int dir, int player, FILE *fp) {
	while (curr_board[square] == opponent(player, fp)) square = square + dir;
	if (curr_board[square] == player) return square;
	else return 0;
}

int opponent(int player, FILE *fp) {
	if (player == BLACK) return WHITE;
	if (player == WHITE) return BLACK;
	fprintf(fp, "illegal player\n"); return EMPTY;
}

int random_strategy(int *curr_board, int my_colour, FILE *fp) {
	int r;
	int *moves = (int *) malloc(LEGALMOVSBUFSIZE * sizeof(int));
	memset(moves, 0, LEGALMOVSBUFSIZE);

	legal_moves(curr_board, my_colour, moves, fp);
	if (moves[0] == 0) {
		return -1;
	}
	srand (time(NULL));
	r = moves[(rand() % moves[0]) + 1];
	free(moves);
	return(r);
}

void make_move(int *curr_board, int move, int player, FILE *fp) {
	int i;
	curr_board[move] = player;
	for (i = 0; i <= 7; i++) make_flips(curr_board, move, ALLDIRECTIONS[i], player, fp);
}

void make_flips(int *curr_board, int move, int dir, int player, FILE *fp) {
	int bracketer, c;
	bracketer = would_flip(curr_board, move, dir, player, fp);
	if (bracketer) {
		c = move + dir;
		do {
			curr_board[c] = player;
			c = c + dir;
		} while (c != bracketer);
	}
}

void print_board(FILE *fp) {
	int row, col;
	fprintf(fp, "   1 2 3 4 5 6 7 8 [%c=%d %c=%d]\n",
		nameof(BLACK), count(board, BLACK), nameof(WHITE), count(board, WHITE));
	for (row = 1; row <= 8; row++) {
		fprintf(fp, "%d  ", row);
		for (col = 1; col <= 8; col++)
			fprintf(fp, "%c ", nameof(board[col + (10 * row)]));
		fprintf(fp, "\n");
	}
	fflush(fp);
}

char nameof(int piece) {
	assert(0 <= piece && piece < 5);
	return(piecenames[piece]);
}

int count(int *curr_board, int player) {
	int i, cnt;
	cnt = 0;
	for (i = 1; i <= 88; i++)
		if (curr_board[i] == player) cnt++;
	return cnt;
}

/*
 *  Finds either the maximum or the minimum utility
 * 	based on the layer of the minimax tree
 */
int find_maxmin(int maxlayer, int *util, int amount_moves) {
	int maxmin;
	if(maxlayer == 0){
		maxmin = -1000;
	} else {
		maxmin = 1000;
	}
	for (int i = 0; i < amount_moves; i++) {
		if (util[i] != PRUNE){
			if (maxlayer == 0) {
			if (util[i] > maxmin) {
				maxmin = util[i];
			}
		} else {
			if (util[i] < maxmin) {
				maxmin = util[i];
			}
		}
		}
	 
	}
	return maxmin;
}

/*
*	Creates a new board in memory that is an exact
*	duplicate of the board int the input parameter
*	up to n bytes
*/
int * duplicate_board(int *curr_board, size_t n){

int *new_board = (int *) malloc(n * sizeof(int));
memcpy(new_board,curr_board,n);
return new_board; 
}

int calc_util (int *curr_board, int curr_player,
int curr_bcount, int curr_wcount, int curr_depth){

int util = 0;
int b_diff = count(curr_board, BLACK) - curr_bcount;
int w_diff = count(curr_board, WHITE) - curr_wcount;
if (curr_player == BLACK){
	util = b_diff - w_diff;
} else {
	util = w_diff - b_diff;
}	
	if(curr_depth % 2 == 0 && curr_depth != DEPTH) {
	if (curr_bcount + curr_wcount < 25){
		util = util + curr_depth;
	} else if (curr_bcount + curr_wcount < 50){
		util = util + curr_depth * 2;
	} else {
		util = util + curr_depth * 3;
	}
	}


return util;
}

/*
*	Creates a game tree that uses the minmax algorithm with
*	alpha-beta pruning to find the optimal move under the assumption
*	that the other player is also using an optimal strategy
*/
int minmax(int *curr_board, int *moves, int depth, int curr_layer,
int curr_player,int curr_bcount, int curr_wcount, int alpha,
int beta, int maxmin, int first_player, FILE *fp) {
	int util = 0;
	int *new_moves;
	int amount_moves = moves[0];
	int next_player = 0;
	if(curr_player == BLACK){
		next_player = WHITE;
	} else {
		next_player = BLACK;
	}
	if (curr_layer != 0) {
		new_moves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
		legal_moves(curr_board,curr_player,new_moves,fp);
	} else if (curr_layer != depth) {
		new_moves = duplicate_board(moves, LEGALMOVSBUFSIZE * sizeof(int));
	}

	if (depth > curr_layer && amount_moves != 0) {

		int *util_arr = (int *)malloc(amount_moves * sizeof(int));
		int i;
		for(i = 1; i <= amount_moves; i++){
			if (alpha >= beta) {
				for (i = i; i <= amount_moves; i++){
					util_arr[i-1] = PRUNE;
				}
			}
			int *sim_board = duplicate_board(curr_board,BOARDSIZE);
			make_move(sim_board, moves[i], curr_player, fp);
			util_arr[i-1] = minmax(sim_board, new_moves, depth, curr_layer+1
			,next_player, curr_bcount, curr_wcount, alpha, beta, !maxmin, first_player, fp);
			free(sim_board);
			fflush(stdout);
		} 
		util = find_maxmin(maxmin, util_arr, amount_moves);
		if(maxmin == 0){
			if (util > alpha){
				alpha = util;
			}
		} else {
			if (util < beta){
				util = beta;
			}
		}
		if(curr_layer != 0){
			free(new_moves);
		}
		free(util_arr);
		
	}else {
		util = calc_util(curr_board, first_player, curr_bcount, curr_wcount,curr_layer);
	}
	return util;
}





