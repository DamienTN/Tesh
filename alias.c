#include <stdlib.h>
#include <string.h>
#include "alias.h"

typedef struct AliasList{
	char *key;
	char *value;
	struct AliasList *left,*right;
}AliasList;

AliasList *root = NULL;

char *search(AliasList *node, char *s){
	if(node==NULL)
		return s;
	int c = strcmp(node->key,s);
	switch(c){
		case 0:
			return node->value;
		case 1:
			return search(node->left,s);
		case -1:
			return search(node->right,s);
		default:
			return s;
	}
	return s;
}

char * getAlias(char *s){
	if(root==NULL)
		return s;
	else
		return search(root,s);
}

void insertAlias(AliasList *node,char *s, char *alias){
	if(node == NULL){
		node = malloc(sizeof(AliasList));
		
		node->key = malloc( (strlen(s)+1)*sizeof(char));
		strcpy(node->key,s);
		
		node->value = malloc( (strlen(alias)+1)*sizeof(char));
		strcpy(node->value,alias);
		
		node->left = NULL;
		node->right = NULL;
		
		return;
	}
	int c = strcmp(node->key,s);
	if(c>0)
		insertAlias(node->left,s,alias);
	else if(c<0)
			insertAlias(node->right,s,alias);
}

void addAlias(char *s, char *alias){
	insertAlias(root,s,alias);
}

void deleteAliasList(AliasList *node){
	if(node!=NULL){
		deleteAliasList(node->left);
		deleteAliasList(node->right);
		free(node->value);
		free(node->key);
		free(node);
	}
}

void endAlias(){
	deleteAliasList(root);
}