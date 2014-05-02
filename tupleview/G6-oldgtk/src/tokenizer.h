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

int getToken(FILE *fp,char *token,int buflen);
int sgetToken(char **str,char *token,int buflen);


#endif 	    /* !TOKENIZER_H_ */
