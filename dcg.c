/*
 Last Modified: July 30, 2012
 
 dcg.c
 
 To compile: gcc -Wall dcg.c -o prog -lm
 To run: ./prog inputFile outputFile minShare MAXITEMS MAXITEMSETS

 Increase MAXITEMSETS if get seg fault
 May need to increase MAXITEMSETS if minshare really small
 
 Set MAXITEMS equal to exact number of items there are, if
 set to less than this will not get correct results.
 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#define MAXFREQUENT			500

#define DEBUG     			0
#define PRT_FP    			1	//print frequent patterns
#define PRT_CNT			0	//print cardinality of sets
#define PRT_MEM   			0	//print memory consumption
#define PRT_FALSE_POS		0	//print number of false positives
#define SHOW_TRAVERSAL_INFO 	0
#define ROOT				-1

#define MIN_LMV(x, y) 	((int)(ceil((x)*(y))))

int MAXITEMS;		//set to exact number of items
int MAXITEMSETS;	//increase this if seg fault

typedef enum BOOL{
	false = 0,
	true = 1
} boolean;

//the nodes in the "trie"
typedef struct node{
	int item;
	int count;
	int lmv;
	boolean last_item;
	struct node *right_sibling;
	struct node *left_sibling;
	struct node *first_child;
	struct node *last_child;
	struct node *parent;
	struct tmv *info;
} TreeNode;

typedef struct tmv{
	int *tmvarray;
} CandidateTmv;


typedef struct{

	int *itemset;
	int lmv;
	int *Tmvdb;
} Itemset;

typedef struct{

	int *itemset;
	int total_count;
   
} Transaction;


void init_itemset(Itemset *I){
	
	I->itemset = malloc((MAXITEMS + 1) * sizeof(int));
	I->Tmvdb = malloc((MAXITEMS + 1) * sizeof(int));
	int i;
	for(i = 1; i <= MAXITEMS; i ++){
		I->itemset[i] = 0;
		I->Tmvdb[i] = 0;
	}
	I->lmv = 0;
}

void init_transaction(Transaction *T){
	
	T->itemset = malloc((MAXITEMS + 1) * sizeof(int));
	int i;
	for(i = 1; i <= MAXITEMS; i ++){
		T->itemset[i] = 0;
	}
	T->total_count = 0;
}

void free_itemset(Itemset *I){
	
	if(I->itemset != NULL){
		free(I->itemset);
		free(I->Tmvdb);
		I->itemset = NULL;
		I->Tmvdb = NULL;
	}
}

void free_transaction(Transaction *T){

	if(T->itemset != NULL){
		free(T->itemset);
		T->itemset = NULL;
	}
}

CandidateTmv *create_info(){
	CandidateTmv *T;

	T = (CandidateTmv *)malloc(sizeof(CandidateTmv));
	if(T == NULL){
		fprintf(stderr, "ERROR[create_info]\n");
		exit(0);
	}
	T->tmvarray = malloc((MAXITEMS + 1) * sizeof(int));
	int i;
	for(i = 1; i <= MAXITEMS; i ++){
		T->tmvarray[i] = 0;
	}

	return T;
}

TreeNode *create_new_node(int item){

	TreeNode *N;
	N = (TreeNode *)malloc(sizeof(TreeNode));
	if(N == NULL){
		fprintf( stderr, "ERROR[create_new_node]\n" );
		exit( 0 );
	}
	N->item = item;
	N->count = 1;
	N->lmv = 0;
	N->last_item = false;
	N->right_sibling = NULL;
	N->left_sibling = NULL;
	N->first_child = NULL;
	N->last_child = NULL;
	N->parent = NULL;
	N->info = NULL;
	
	return N;
}

boolean is_root(TreeNode *X){

	boolean result = false;
	if(X != NULL && X->item == ROOT){
		result = true;
	}
	return result;

}

void free_array(TreeNode *set[], int *size){

	int i;
	for(i = 0; i < *size; i ++){
		set[i] = NULL;
	}
	(*size) = 0;
	
}

void remove_itemset(TreeNode *X, TreeNode *set[], int pos){

	while(is_root(X) == false){
		TreeNode *temp = X->parent;
		
		if(X->count == 1){
			
			if(X->left_sibling != NULL && X->right_sibling != NULL){
				
				//readjusting pointers and also removing dangling ones
				(X->left_sibling)->right_sibling = X->right_sibling;
				(X->right_sibling)->left_sibling =  X->left_sibling;
			}
			else if(X->left_sibling == NULL){ //means it is a first child
				(X->parent)->first_child = X->right_sibling;
				if(X->right_sibling != NULL){
					(X->right_sibling)->left_sibling = NULL;
				}
			}
			else if(X->right_sibling == NULL){
				(X->left_sibling)->right_sibling = NULL;
				(X->parent)->last_child = X->left_sibling;
			}
			free(X);
		}
		else if(X->count > 1){
			(X->count) --;
		}
		X = temp;
	}
	
	set[pos] = NULL;

}

void free_tree(TreeNode *set[], int *size){

	int i;
	for(i = 0; i < *size; i ++){
		remove_itemset(set[i], set, i);
	}
	free_array(set, size);

}

void copy_tree(TreeNode *rootSrc, TreeNode *setSrc[], int *sizeSrc, TreeNode *rootDest, TreeNode *setDest[], int *sizeDest){

	int i;
	TreeNode *first = rootSrc->first_child;
	free_tree(setDest, sizeDest);
	
	for(i = 0; i < *sizeSrc; i ++){
		setDest[i] = setSrc[i];
		setSrc[i] = NULL;
	}
	*sizeDest = *sizeSrc;
	*sizeSrc = 0;
	
	rootDest->first_child = first;
	rootSrc->first_child = NULL;

}

TreeNode *match_child(int item, TreeNode *node){
	
	TreeNode *X = node->first_child;
	
	while(X != NULL && X->item != item){
		X = X->right_sibling;
	}
	return X;
}

void to_itemset(Itemset *I, TreeNode *X){
	
	init_itemset(I);
	while(X != NULL && is_root(X) == false){
		I->itemset[X->item] = 1;
		X = X->parent;
	}

}

int get_lmv(Transaction *T, TreeNode *N){

	TreeNode *X = N;
	int lmv = 0;
	while(is_root(X) == false){
		lmv += T->itemset[X->item];
		X = X->parent;
	}
	
	return lmv;
}

void print_itemset(TreeNode *X){
	
	printf("{ ");
	while(is_root(X) == false){
		printf("%d ", X->item);
		X = X->parent;
	}
	printf("}");

}

void traverse(TreeNode *set[], int size){

	int i, j;
	for(i = 0; i < size; i ++){
		printf("\n");
		TreeNode *X = set[i];
		print_itemset(X);
		if(SHOW_TRAVERSAL_INFO){
			printf(" count: [%d] lmv: [%d] ", X->count, X->lmv);
			printf("tmv array: [ ");
			for(j = 1; j <= MAXITEMS; j ++){
				printf("%d ", X->info->tmvarray[j]);
			}
			printf("] ");
		}
	}

}

void print_frequent_itemset(TreeNode *set[], int size, FILE *f_output){

	int i;
	for(i = 0; i < size; i ++){
		printf("\n");
		fprintf(f_output, "\n");
		TreeNode *X = set[i];

		printf("{ ");
		fprintf(f_output, "{ ");
		while(is_root(X) == false){
			printf("%d ", X->item);
			fprintf(f_output, "%d ", X->item);
			X = X->parent;
		}
		printf("}");
		fprintf(f_output, "}");
	}
}

void count_nodes(TreeNode *X, int *counter){

	if(X == NULL){
		return;
	}
	count_nodes(X->right_sibling, counter);
	//printf("%d ", X->item);
	(*counter) ++;
	count_nodes(X->first_child, counter);

}

boolean is_member(int itemset[], TreeNode *N){
	

	int found = true;
	TreeNode *X = N;
	while(is_root(X) == false && found == true){
		if(itemset[X->item] == 0){
			found = false;
		}
		X = X->parent;
	}

	return found;

}

//don't need first item in dcg
void insert(int itemset[], TreeNode *node, int current_item, int *size, TreeNode *set[], int lmv){

	TreeNode *N;
	int value = itemset[current_item];
	
	if(value == 1){
		if(node->first_child == NULL){
			N = create_new_node(current_item);
			N->parent = node;
			node->first_child = N;
			node->last_child = N;

		}
		else{
			N = match_child(current_item, node);
			if(N == NULL){
				N = create_new_node(current_item);
				N->parent = node;
				node->last_child->right_sibling = N;
				N->left_sibling = node->last_child;
				node->last_child = N;

			}
			else{
				(N->count) ++;
			}
		}
	}
	else{
		N = node;
	}
	if(current_item == MAXITEMS && is_root(N) == false){
		N->last_item = true;
		N->lmv = lmv;
		CandidateTmv *info = create_info();
		N->info = info;
		set[*size] = N;
		(*size) ++;
	}

	if(current_item < MAXITEMS){
		insert(itemset, N, current_item + 1, size, set, lmv);
	}

}

int main(int argc, char *argv[]){

	struct timeval  start_time, end_time; /* structs for timer     */
	struct timezone zone;
	long int sec = 0, usec = 0; /* sec & microsec for the timer    */
	
	long int time_total_sec = 0;
     double time_total_msec = 0;
     long int time_total_usec = 0;

	int numTrans, pid, numItems, item, count, i , j;
	FILE *f_input, *f_output;
	
	double minshare;
	int sizeDB;
	
	if(argc != 6){
		fprintf(stderr, "Usage: %s inputFile outputFile minshare MAXITEMS MAXCANDIDATES\n", argv[0]);	
		exit(0);
	}
	
	f_input = fopen(argv[1], "r");
	f_output = fopen(argv[2], "w");
	minshare = atof(argv[3]);
	MAXITEMS = atoi(argv[4]);
	MAXITEMSETS = atoi(argv[5]);
	
	if((f_input == NULL) || (f_output == NULL)){
		fprintf(stdout, "ERROR[%s]: Can't open %s or %s\n", argv[0], argv[1], argv[2]);
    		fprintf(stderr, "ERROR[%s]: Can't open %s or %s\n", argv[0], argv[1], argv[2]);
    		exit( 0 );
	}
	
	int lmv[MAXITEMS + 1];
	int tmv[MAXITEMSETS];
	int Tmv_DB = 0;

	TreeNode *rootCk;
	rootCk = create_new_node(ROOT);

	TreeNode *rootRc;
	rootRc = create_new_node(ROOT);
	
	TreeNode *rootF;	//the tree for frequent itemsets
	rootF = create_new_node(ROOT);
	
	TreeNode *setRc[MAXITEMSETS + 1];
	TreeNode *setCk[MAXITEMSETS + 1];
	TreeNode *setF[MAXITEMSETS + 1];
	
	int sizeRc = 0;
	int sizeCk = 0;
	int sizeF = 0;
	
	for(i = 1; i <= MAXITEMS; i ++){
		lmv[i] = 0;
	}
	for(i = 1; i <= MAXITEMSETS; i ++){
		tmv[i] = 0;
	}
	
	printf("===== %s %s %s %f =====\n\n", argv[0], argv[1], argv[2], minshare);
	
     //record the time for the first db scan
     if(gettimeofday(&start_time, &zone) == -1){
       fprintf(stderr, "gettimeofday error\n");
     }
	
	fscanf(f_input, "%d ", &numTrans);
	int total_count = 0;

	//read the whole db once to get candidate 1-itemsets
	for(i = 1; i <= numTrans; i ++){
		//printf("\n");
	
		fscanf(f_input, "%d ", &pid);
		fscanf(f_input, "%d ", &numItems);
		//printf("%d %d ", pid, numItems);
		for(j = 1; j <= numItems; j ++){
			fscanf(f_input, "%d %d ", &item, &count);
			//printf("%d %d ", item, count);
			lmv[item] += count;
			total_count += count;
			Tmv_DB += count;
		}
		tmv[i] = total_count;
		total_count = 0;
	}
	sizeDB = numTrans;
	
	if(gettimeofday(&end_time, &zone) == 0){
      	if(end_time.tv_usec >= start_time.tv_usec){
      		sec  = end_time.tv_sec - start_time.tv_sec;
      		usec = end_time.tv_usec - start_time.tv_usec;
      	}else{
      		sec  = end_time.tv_sec - start_time.tv_sec - 1;
      		usec = end_time.tv_usec - start_time.tv_usec + 1000000;
      	}
      	time_total_sec += sec;
      	time_total_usec += usec;
      	
      	fprintf(stdout, "\n[DCG] Total runtime for first db scan is %ld sec. %.3f msec\n", sec, usec/1000.0);
      	f_output = fopen( argv[2], "a" );
      	fprintf(f_output, "\n[DCG] Total runtime for first db scan is %ld sec. %.3f msec\n", sec, usec/1000.0);
      	fclose( f_output );
      }
	
	if(DEBUG){
		printf("\nTmv: %d Min_LMV: %d", Tmv_DB, MIN_LMV(minshare, Tmv_DB));
	}

	//get candidate 1-itemsets
	for(i = 1; i <= MAXITEMS; i ++){
		if(lmv[i] != 0){
		
			Itemset I;
			init_itemset(&I);
			I.itemset[i] = 1;
			insert(I.itemset, rootRc, 1, &sizeRc, setRc, 0);
			free_itemset(&I);
		}
	}
	if(PRT_CNT){
		printf("\n\nIteration (%d)", 0);
		printf("\n|Rc| = %d (initial set of candidates)", sizeRc);
		printf("\n|Fk| = %d", sizeF);
	}

	int last;
	int k, t, c;
	int counterF = 0;
	boolean exit = false;
	int num_false_positives[MAXITEMSETS + 1];
	int sizeFP = 0;

	//record the time for mining
	if(gettimeofday(&start_time, &zone) == -1){
		fprintf(stderr, "gettimeofday error\n");
	}
	for(k = 1; k <= MAXITEMS && (exit == false); k ++){

		if(k > 1){
			copy_tree(rootCk, setCk, &sizeCk, rootRc, setRc, &sizeRc);
		}
		rewind(f_input);
		fscanf(f_input, "%d ", &numTrans);

		for(t = 1; t <= sizeDB; t ++){

			//scan the next transaction, put in struct T to process more easily
			Transaction T;
			fscanf(f_input, "%d ", &pid);
			fscanf(f_input, "%d ", &numItems);
	
			init_transaction(&T);
			for(j = 1; j <= numItems; j ++){
				fscanf(f_input, "%d %d ", &item, &count);
				
				(T.itemset[item]) += count;
				(T.total_count) += count;
			}
			for(c = 0; c < sizeRc; c ++){
				TreeNode *X = setRc[c];
				if(is_member(T.itemset, X)){

					X->lmv += get_lmv(&T, X);
				}
				last = X->item;

				for(j = last + 1; j <= MAXITEMS; j ++){
					if(T.itemset[j] != 0 && is_member(T.itemset, X)){
						X->info->tmvarray[j] += tmv[t];
					}
				}
			}
			if(t == sizeDB){
				for(c = 0; c < sizeRc; c ++){
					TreeNode *X = setRc[c];
					if(X->lmv >= MIN_LMV(minshare, Tmv_DB)){
						counterF ++;
						Itemset I;
						to_itemset(&I, X);
						insert(I.itemset, rootF, 1, &sizeF, setF, X->lmv);
						free_itemset(&I);
					}
					last = X->item;
					for(j = last + 1; j <= MAXITEMS; j ++){

						if(X->info->tmvarray[j] >= MIN_LMV(minshare, Tmv_DB)){

							Itemset I;
							to_itemset(&I, X);
							I.itemset[j] = 1;
							insert(I.itemset, rootCk, 1, &sizeCk, setCk, 0);
							free_itemset(&I);
						}
					}
				}
			}
			free_transaction(&T);
		}
		if(PRT_MEM){
			printf("\n\nIteration (%d)", k);
			int mem_counterCk = 0;
			count_nodes(rootCk, &mem_counterCk);
			printf("\nNode Count = [%d]", mem_counterCk);
			printf("\nNode Size = [%ld bytes]", sizeof(TreeNode));
			printf("\nMemory space required for candidate tree = %ld bytes", mem_counterCk * sizeof(TreeNode));
		}
		if(PRT_CNT){
			printf("\n\nIteration (%d)", k);
			printf("\n|Ck| = %d", sizeCk);
			printf("\n|Fk| = %d (number of frequent patterns from Ck from iteration (%d)", counterF, k-1); //the frequent itemsets from the previous set Ck (Ck-1)
		}
		if(PRT_FALSE_POS){
			if(k > 1){
				num_false_positives[k - 1] -= counterF;
				printf("\nFalse Positives Iteration (%d): %d", k-1, num_false_positives[k-1]);
			}
			num_false_positives[k] = sizeCk;
			sizeFP ++;
		}
		if(DEBUG){
			traverse(setCk, sizeCk);
			printf("\nsizeCk: %d", sizeCk);
			fflush(stdout);
		}
		if(sizeCk == 0){
			exit = true;
		}
		counterF = 0;
	}
	if(gettimeofday(&end_time, &zone) == 0){
	 	if(end_time.tv_usec >= start_time.tv_usec){
	 		sec  = end_time.tv_sec - start_time.tv_sec;
	 		usec = end_time.tv_usec - start_time.tv_usec;
	 	}else{
	 		sec  = end_time.tv_sec - start_time.tv_sec - 1;
	 		usec = end_time.tv_usec - start_time.tv_usec + 1000000;
	 	}
      	time_total_sec += sec;
      	time_total_usec += usec;
      	
	 	fprintf(stdout, "\n[DCG] Total runtime for mining is %ld sec. %.3f msec\n", sec, usec/1000.0);
	 	f_output = fopen( argv[2], "a" );
	 	fprintf(f_output, "\n[DCG] Total runtime for mining is %ld sec. %.3f msec\n", sec, usec/1000.0);
	 	fclose( f_output );
 	}
 	
	f_output = fopen(argv[2], "a");
	if(PRT_FALSE_POS){
		int total_FP = 0;
		for(k = 1; k <= sizeFP; k ++){
			total_FP += num_false_positives[k];
		}
		printf("\nTotal False Positives: %d", total_FP);
	}
	if(PRT_FP){
		//see the result
		printf("\n\nFound (%d) ShFrequent Itemsets:", sizeF);
		fprintf(f_output, "\n\nFound (%d) ShFrequent Itemsets:", sizeF);
		print_frequent_itemset(setF, sizeF, f_output);
	}
	if(PRT_MEM){
		int mem_counterF = 0;
		count_nodes(rootF, &mem_counterF);
		printf("\n\nNode Count = [%d]", mem_counterF);
		printf("\nNode Size = [%ld bytes]", sizeof(TreeNode));
		printf("\nMemory space required for frequent itemset tree = %ld bytes", mem_counterF * sizeof(TreeNode));
	}
	
	time_total_msec = time_total_usec / 1000.0;
  	if(time_total_msec >= 1000){
  		time_total_sec += floor(time_total_msec/1000);
  		time_total_msec = time_total_usec % 1000;
  	}
  	
  	//printf("\ntime sec: %ld time msec: %.3lf time usec: %ld", time_total_sec, time_total_msec, time_total_usec);

	fprintf(stdout, "\n\n[DCG] Total (aggregate) runtime is %ld sec. %.3lf msec\n", time_total_sec, time_total_msec);
	fprintf(f_output, "\n\n[DCG] Total (aggregate) runtime is %ld sec. %.3lf msec\n", time_total_sec, time_total_msec);

	free_tree(setRc, &sizeRc);
	free_tree(setCk, &sizeCk);
	free_tree(setF, &sizeF);
	fclose(f_input);
	fclose(f_output);
	printf("\n\nProcessing Complete\n");
	return 0;
}

