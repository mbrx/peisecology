/*
** tokenizer.h
** 
** Made by Mathias Broxvall
** Login   <mbl@brox>
** 
** Started on  Wed Mar 12 18:31:24 2008 Mathias Broxvall
** Last update Wed Mar 12 18:31:24 2008 Mathias Broxvall
*/

#ifndef   	TOKENIZER_H_
# define   	TOKENIZER_H_

extern int lineno;

#define TOKEN_EOF  -1
#define TOKEN_ERROR 0

/** \brief Inserts a token into list of all global tokens. \return Returns integer representation of token. */
int insertToken(char *);
/** \brief Gives the string representation of a given token. */
char *token2string(int);

/** \brief Initializes the tokenizer */
void initTokenizer();

/** \brief Reads another token from the given file. \param fp The input file \return Returns a positive integer token representation of it, or zero on error */
int getToken(FILE *fp,int *lineno);
/** \brief Reads another token from and updates the given string pointer. \param str Pointer to a pointer to a string containing the inputs \return Returns a positive integer token representation of it, or zero on error */
int sgetToken(char **str,int *lineno);
/** \brief Reads another token from stdin using the GNU readline library. */
int readlineToken(int *lineno);
extern char *readlinePromptType;

/** \brief Discards any left-over readline inputs eg. after an error in the program */
void flushReadline();

/** \brief Current line number. Is updated by tokenzier, but expected to be initalized by caller. */
extern int lineno;


#endif 	    /* !TOKENIZER_H_ */


