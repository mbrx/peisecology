/** \file ops_containers.c
    \brief Defines all container (list, dictionaries etc) operators.
*/
      case CONS:
	value=valueStack[valueStart];
	value2=valueStack[valueStart+1];
	valueStack[valueStart]=cons2value();
	valueStack[valueStart]->data.cons.first = value;
	valueStack[valueStart]->data.cons.rest = value2;
	valueDepth=valueStart+1;
	break;

      case FIRST_SET:
      case REST_SET:
	value=valueStack[valueStart];
	value2=valueStack[valueStart+1];
	if(value->kind != eCons) {
	  fprintf(stderr,"%s:%d - Invalid datatype in %s operation, expected a cons-cell\n",
		  program->name,opStack[opDepth-1].lineno,token2string(operand));
	  return;
	}
	if(operand == FIRST_SET) {
	  value3=value->data.cons.first;
	  value->data.cons.first=value2;
	  reference(value2);
	  unreference(value3);
	} else {
	  value3=value->data.cons.rest;
	  value->data.cons.rest=value2;
	  reference(value2);
	  unreference(value3);
	}
	valueDepth=valueStart;
	break;
      case FIRST:
      case REST:
	value=valueStack[valueStart];
	if(value->kind != eCons) {
	  fprintf(stderr,"%s:%d - Invalid datatype in %s operation, expected a cons-cell\n",
		  program->name,opStack[opDepth-1].lineno,token2string(operand));
	  return;
	}
	valueStack[valueStart]= operand==FIRST?value->data.cons.first:value->data.cons.rest;
	reference(valueStack[valueStart]);
	unreference(value);
	valueDepth=valueStart+1;
	break;

      case LIST: 
	{
	  /* Create a list of _all_ arguments given to this function */
	  value2=NULL;
	  while(valueDepth > valueStart) {
	    value=valueStack[--valueDepth];
	    value3=cons2value(); 
	    value3->data.cons.first=value;
	    value3->data.cons.rest=value2;
	    value2=value3; 
	  }
	  valueStack[valueStart]=value2;
	  valueDepth=valueStart+1;
	}
	break;
