/** \file ops_control.c
    \brief Defines all the control operators and variable manipulations. 
*/

 case DEFUN:
 case LAMBDA: {
   LambdaObject *lobj = malloc(sizeof(struct LambdaObject));
   lobj->program=programsVal[frameDepth];
   lobj->environment=environment;
   Value *lambda = lambda2value(lobj);
   reference(lobj->program);
   reference(lobj->environment);
   /* Read name of function (if applicable) */
   int functionName;
   if(operand == DEFUN) {
     i2=getInstruction();
     if(!i2 || isReserved(i2->token)) { fprintf(stderr,"%s:%d -- invalid function name '%s'\n",program->name,i2->lineno,token2string(i2->token)); return; }
     functionName=i2->token;
   }
   /* Read over argument list and count number of args */
   int nargs=0;
   i2=getInstruction();
   if(!i2 || i2->token != L_PARAN) { fprintf(stderr,"%s:%d -- %d Syntax error - expected '(' before argument list, not '%s'\n",program->name,instruction->lineno,i2->lineno,token2string(i2->token)); return; }
   lobj->pcstart = pc[frameDepth];
   while(1) {
     i2=getInstruction();
     if(!i2) { fprintf(stderr,"%s:%d -- %d Syntax error - expected closing ')''\n",program->name,instruction->lineno,i2->lineno); return; }
     if(i2->token == R_PARAN) break;
     else if(isReserved(i2->token)) { fprintf(stderr,"%s:%d - Illegal variable name '%s' here\n",program->name,i2->lineno,token2string(i2->token)); return; }	    
     else nargs++;
   } 
   lobj->nargs=nargs;
   i2=getInstruction();
   if(i2->token == LC_BRACKET) skipBlock(instruction);
   else { fprintf(stderr,"%s:%d Syntax error - expected '{' here\n",program->name,i2->lineno); return; }
   if(operand == DEFUN) {
     bindFunction(environment,functionName,lambda);
     unreference(lambda);
     valueDepth=valueStart;
   } else {
     valueStack[valueStart]=lambda;
     valueDepth=valueStart+1;
   }
 } break;
 case CALL: {
   Value *lambda = valueStack[valueStart];
   if(lambda->kind != eLambda) { fprintf(stderr,"%s:%d - Expected lambda object here, got: ",program->name,instruction->lineno); fprintfValue(stderr,lambda); fprintf(stderr,"\n"); return; }
   LambdaObject *lobj = lambda->data.lambda;
   /** Check that we have enough arguments  */
   if(argNum < lobj->nargs+1) goto cont_running;
   /** Jump into the program object given by the lambda function */
   frameDepth++;
   programsVal[frameDepth]=lobj->program;
   programs[frameDepth]=lobj->program->data.program;
   reference(lobj->program);
   pc[frameDepth]=lobj->pcstart;	    
   /** Bind arguments to values */
   Value *oldEnv =environment;
   environment = environment2value(NULL,peisk_hashTable_create(PeisHashTableKey_Integer));
   createLocalBinding(environment,_TOP,lobj->environment);	  
   control[controlDepth].kind = eFunction;
   for(i=1;;i++) {
     i2=getInstruction();
     // No checking on i2 neccessary since it was done when the lambda object was created 
     if(i2->token == R_PARAN) break;
     /* Bind a value to this argument */
     //printf("Binding %s to ",token2string(i2->token)); fprintfValue(stdout,valueStack[valueStart+i]); printf("\n");
     createLocalBinding(environment,i2->token,valueStack[valueStart+i]);
     unreference(valueStack[valueStart+i]);
   }
   control[controlDepth].opStart = opDepth-1;
   control[controlDepth].origin = pc[frameDepth];
   control[controlDepth].originFrameDepth = frameDepth;
   control[controlDepth].iterant = oldEnv;
   controlDepth++;
   valueDepth=valueStart;	  	 
   i2=getInstruction(); // get rid of opening '{'
   unreference(lambda); // we do no longer need the lambda object 
 } break;
/* Assignment to global and local variables */
 case LOCAL:
 case LET: {
   key=valueStack[valueStart];
   value=valueStack[valueStart+1];
   if(operand==LOCAL) createLocalBinding(environment,key->data.token,value);
   else assignVariable(environment,key->data.token,value);
   unreference(key); 
   unreference(value);
   valueDepth=valueStart;
 } break;

/* Iterating over lists and resultsets */
 case FOR: {
   key=valueStack[valueStart];     // Variable to iterate with
   value=valueStack[valueStart+2]; // Variable to iterate over
   if(key->kind != eAtom) { fprintf(stderr,"For-loop varialbe is not an atom. Bug in interpreter?\n"); fprintfValue(stdout,key); printf("\n"); return; }
   if(repl) { indentation++; makePrompt(); }
   control[controlDepth].kind = eForLoop;
   control[controlDepth].controlVariable = key->data.token;
   control[controlDepth].origin = pc[frameDepth];
   control[controlDepth].originFrameDepth = frameDepth;
   control[controlDepth].opStart = opDepth-1;
   control[controlDepth].iterant = value;       /* control stack is inherting our reference to the value. */
   controlDepth++;
   /* Skip until end of block, and let iteration start with the RC_BRACKET */
   skipBlock(instruction); 
   pc[frameDepth]--;
   
   unreference(valueStack[valueStart+0]);
   unreference(valueStack[valueStart+1]);
   unreference(valueStack[valueStart+3]);
   valueDepth=valueStart;
   if(repl) { indentation++; makePrompt(); }
 } break;
 case IF: {
   value=valueStack[valueStart];
   control[controlDepth].kind = eIfStatement;
   control[controlDepth].opStart=opDepth-1;
   control[controlDepth].origin=pc[frameDepth];
   control[controlDepth].originFrameDepth = frameDepth;
   control[controlDepth].boolean=value2boolean(value);
   if(!control[controlDepth].boolean) {
     //printf("Skipping\n");
     skipBlock(instruction);
     pc[frameDepth]--;
   }
   controlDepth++;
   unreference(value);
   unreference(valueStack[valueStart+1]); // get rid of the curly bracket
   valueDepth=valueStart;
 } break;
 case ELSE: {
   control[controlDepth].kind = eElseStatement;
   control[controlDepth].opStart=opDepth-1;
   control[controlDepth].origin=pc[frameDepth];
   control[controlDepth].originFrameDepth = frameDepth;
   controlDepth++;
   unreference(valueStack[valueStart]); // get rid of the curly bracket
   valueDepth=valueStart;
 } break;

/* Empty block without any associated specific control flow */
 case L_PARAN:
 case LC_BRACKET: {
   
   if(repl) { indentation++; makePrompt(); }
   control[controlDepth].kind = eBlock;
   control[controlDepth].origin=pc[frameDepth];
   control[controlDepth].originFrameDepth = frameDepth;
   control[controlDepth].opStart = opDepth-1;
   controlDepth++;
 } break;

/* Closing a block */
 case R_PARAN:
 case RC_BRACKET: {
   /* Close any previous controlflow/block sections */
   if(repl) { indentation--; makePrompt(); }
   controlDepth--;
   switch(control[controlDepth].kind) {
   case eBlock: /* Ended a stand-alone block. Nothing else is needed to be done */ break;
   case eIfStatement:
     /* Check if the next statement is an ELSE statement */
     i2=getInstruction(); 
     if(i2 && i2->token != ELSE) {
       /* The IF statement had no corresponding ELSE, push back read instruction and continue. */
       pc[frameDepth]--;
     } else if(i2 && i2->token == ELSE) {
       /* Found the corresponding ELSE instruction. */
       EXPECT(LC_BRACKET,"Expected '{' after ELSE statement\n");
       if(repl) { indentation++; makePrompt(); }
       if(control[controlDepth].boolean) {
	 /* It was a true IF statement, so skip this else part */
	 skipBlock(instruction);
       } else {
	 /* It was a false IF statement, execute this part */
	 control[controlDepth].kind = eElseStatement;
	 control[controlDepth].origin = pcStart+1;
	 controlDepth++;
       }
     }
     break;
   case eElseStatement: /* Finishes a previous else block, nothing to do. */
     break;
   case eForLoop:
     value = value2pop(control[controlDepth].iterant);
     if(value->kind == eAtom && value->data.token == EOF) {
       /* For loop is finished */
       unreference(control[controlDepth].iterant);
     } else {
       /* Set variable to new value */
       createLocalBinding(environment,control[controlDepth].controlVariable,value);
       //peiskmt_setStringTuple(token2string(control[controlDepth].controlVariable),value2string(value,val_buf,sizeof(val_buf)));
	    
       /* Return to begining */
       pc[frameDepth] = control[controlDepth].origin;
       controlDepth++;
     }
     unreference(value);
     break;
   case eFunction:
     unreference(programsVal[frameDepth]);
     frameDepth--;       
     unreference(environment);	  
     environment = control[controlDepth].iterant;
     break;
   default:
     fprintf(stderr,"%s:%d -- %d - Unknown controlflow statement (%d) on stack (possible bug in interpreter!)\n",
	     program->name,control[controlDepth].origin,instruction->lineno,control[controlDepth].kind);
   }
 } break;

 case CONTINUE: 
 case AGAIN: {
   value = valueStack[valueStart];
   i=value2int(value);
   //printf("Before. PC %d Control %d\n",pc[frameDepth],controlDepth);
   for(;i>=0;i--) {
     controlDepth--;
     if(controlDepth<0 || control[controlDepth].originFrameDepth != frameDepth) { 
       fprintf(stderr,"%s:%d - Attempting to jump out of all surrounding control blocks\n",
	       program->name,instruction->lineno);
       return;
     }
     if(i != 0) switch(control[controlDepth].kind) {
       case eForLoop:
	 unreference(control[controlDepth].iterant);
	 break;
       case eFunction:
	 // Impossible case
	 break;
       }
   }
   pc[frameDepth]=control[controlDepth].origin;
   valueDepth=valueStart;
   controlDepth++;
   if(operand == CONTINUE) { 
     skipBlock(instruction); 
     pc[frameDepth]--; // Put back ending RC_BRACKET so it will be processed
   }
   //printf("After. PC %d Control %d\n",pc[frameDepth],controlDepth);
 } break;

 case EXIT:
   peiskmt_shutdown();   
   sleep(1);
   exit(0);
   break;
