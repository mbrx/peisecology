/** \ingroup operations */
/** \file ops_io.c
    \brief Built-in operators for standard unix input/output operations */

/** \ingroup operations
    \code echo (data data data) \endcode Prints out the following data values to stdout. \return Nothing 
*/
 case ECHO: 
   for(i=0;i<argNum;i++) {
     value=valueStack[valueStart+i];
     fprintfValue(stdout,value);
     unreference(value);
     valueDepth=valueStart;
   }
   break;
 case NEWLINE: printf("\n"); break;

