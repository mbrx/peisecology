/** \ingroup operations */
/** \file ops_tuples.c
    \brief Defines all operators for accessing and manipuling tuples */

/** \ingroup operations 
    Special operator that construct tuple owner-name cons-cells as arguments to other operators

    \code 1:foo \endcode
*/
 case _TCONS: {
   value=valueStack[valueStart];
   unreference(valueStack[valueStart+1]);
   value2=valueStack[valueStart+2];
   valueStack[valueStart]=cons2value();
   valueStack[valueStart]->data.cons.first = value;
   valueStack[valueStart]->data.cons.rest = value2;
   valueDepth=valueStart+1;
 } break;

/** \ingroup operations 
    Operator for accessing the data part of tuples from our own tuplespace 
    \code $foo \endcode
*/
 case DOLLAR: {
   key=valueStack[valueStart];
   str=value2string(key,key_buf,sizeof(key_buf));
   tuple = peiskmt_getTuple(peiskmt_peisid(),str,PEISK_KEEP_OLD|PEISK_NON_BLOCKING);
   if(!tuple && blocking) {
     printf("%s:%d - Waiting for tuple %d:%s\n",program->name,opStack[opDepth-1].lineno,peiskmt_peisid(),str);
     tuple = peiskmt_getTuple(peiskmt_peisid(),str,PEISK_KEEP_OLD|PEISK_BLOCKING);
   } 
   if(key) unreference(key);
   valueStack[valueStart]=cstring2value(tuple?tuple->data:"");
 } break;

/** \ingroup operations 
    Operator for accessing the data part of singular tuples from remote tuplespaces 
    \code %1:foo \endcode
*/
 case PERCENT: {
   key=valueStack[valueStart];
   if(!key || key->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of '%%' argument. Possible interpreter bug?\n",program->name,opStack[opDepth-1].lineno);
     return;
   }
   o=value2int(key->data.cons.first);
   str=value2string(key->data.cons.rest,key_buf,sizeof(key_buf));
   tuple = peiskmt_getTuple(o,str,PEISK_KEEP_OLD|PEISK_NON_BLOCKING);
   if(!tuple && blocking) {
     printf("%s:%d - Waiting for tuple %d:%s\n",program->name,opStack[opDepth-1].lineno,o,str);
     tuple = peiskmt_getTuple(o,str,PEISK_KEEP_OLD|PEISK_BLOCKING);
   }
   unreference(key);
   if(tuple) valueStack[valueStart]=cstring2value(tuple->data);
   else valueStack[valueStart]=NULL;
 } break;

/** \ingrouop    
    Generic operator for accessing multiple tuples from any tuple spaces 
*/
 case AT: {
   key=valueStack[valueStart];
   if(!key || key->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of '%%' argument. Possible interpreter bug?\n",program->name,opStack[opDepth-1].lineno);
     return;
   }
   o=value2int(key->data.cons.first);
   str=value2string(key->data.cons.rest,key_buf,sizeof(key_buf));
   rs = peiskmt_createResultSet();
   peiskmt_getTuples(o,str,rs);
   peiskmt_resultSetFirst(rs);
   unreference(key);
   valueStack[valueStart]=rs2value(rs);
   valueDepth=valueStart+1;
 } break;

/* Generic tuple address constructor (used together with ! or @) that specifies an indirect tuple */
 case AMPERSAND: {
   
 } break;

 case DOT_OWNER: 
 case DOT_DATA: 
 case DOT_KEY: 
 case DOT_MIMETYPE: {
   value=valueStack[valueStart];
   if(value->kind == eTuple) tuple=value->data.tuple;
   else if(value->kind == eResultSet) {
     peiskmt_resultSetFirst(value->data.rs);
     if(peiskmt_resultSetNext(value->data.rs)) tuple = peiskmt_resultSetValue(value->data.rs);
     else { 
       printf("Failed to get rs next\n");
       valueStack[valueStart]=NULL; unreference(value); break; 
     }
   }
   if(operand == DOT_OWNER) { snprintf(key_buf,sizeof(key_buf),"%d",tuple->owner); valueStack[valueStart]=dstring2value(strdup(key_buf)); }
   else if(operand == DOT_KEY) { peiskmt_getTupleName(tuple,key_buf,sizeof(key_buf)); valueStack[valueStart]=dstring2value(strdup(key_buf)); }
   else if(operand == DOT_DATA) { valueStack[valueStart]=cstring2value(tuple->data); }
   else if(operand == DOT_MIMETYPE) { valueStack[valueStart]=cstring2value(tuple->mimetype); }
   unreference(value);
   valueDepth=valueStart+1;
 } break;

/** \ingroup operations 
 * \code subscribe id:name \endcode Starts a subscription to given abstract tuple. \return Nothing
 */
 case SUBSCRIBE: {
   key=valueStack[valueStart];
   if(!key || key->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of subscribe argument. Possible interpreter bug?\nValue was: ",program->name,opStack[opDepth-1].lineno);
     fprintfValue(stderr,key);
     return;
   }
   o=value2int(key->data.cons.first);
   str=value2string(key->data.cons.rest,key_buf,sizeof(key_buf));
   peiskmt_subscribe(o,str);
   valueDepth=valueStart;
 } break;

/** \ingroup operations 
 * \code set id:name value \endcode Sets the tuple to the given value. \return Nothing
 */
 case SET: {
   key = valueStack[valueStart];
   value = valueStack[valueStart+1];
   if(!key || key->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of SET argument. Possible interpreter bug?\nValue was: ",program->name,opStack[opDepth-1].lineno);
     fprintfValue(stderr,key);
     return;     
   }
   o=value2int(key->data.cons.first);
   str=value2string(key->data.cons.rest,key_buf,sizeof(key_buf));
   v=value2string(value,val_buf,sizeof(val_buf));
   peiskmt_setRemoteStringTuple(o,str,v);
   valueDepth=valueStart;
 } break;

/** \ingroup operations 
 * \code set! id:name value \endcode Sets the tuple to point to given real tuple, blocks until finished. \return Nothing
 */
 case SET_WAIT: {
   key = valueStack[valueStart];
   value = valueStack[valueStart+1];
   if(!key || key->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of SET argument. Possible interpreter bug?\nValue was: ",program->name,opStack[opDepth-1].lineno);
     fprintfValue(stderr,key);
     return;     
   }
   o=value2int(key->data.cons.first);
   str=value2string(key->data.cons.rest,key_buf,sizeof(key_buf));
   v=value2string(value,val_buf,sizeof(val_buf));
   PeisTuple tuple;
   peiskmt_initTuple(&tuple);
   tuple.owner=o;
   peiskmt_setTupleName(&tuple,str);
   tuple.data=v;
   tuple.datalen=strlen(v)+1;
   peiskmt_insertTupleBlocking(&tuple);
   valueDepth=valueStart;
 } break;

/** \ingroup operations 
 * \code set_meta metaid:metaname realid:realname \endcode Sets the meta tuple to point to given real tuple. \return Nothing
 */
 case SET_META: {
   key = valueStack[valueStart];
   value = valueStack[valueStart+1];
   if(!key || key->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of SET_META argument. Possible interpreter bug?\nValue was: ",program->name,opStack[opDepth-1].lineno);
     fprintfValue(stderr,key);
     return;     
   }
   if(!value || value->kind != eCons) {
     fprintf(stderr,"%s:%d - Invalid type of SET_META argument. Possible interpreter bug?\nValue was: ",program->name,opStack[opDepth-1].lineno);
     fprintfValue(stderr,value);
     return;     
   }
   mo=value2int(key->data.cons.first);
   o=value2int(value->data.cons.first);
   mk=value2string(key->data.cons.rest,mkey_buf,sizeof(mkey_buf));
   k=value2string(value->data.cons.rest,key_buf,sizeof(key_buf));
   peiskmt_setMetaTuple(mo,mk,o,k);
   valueDepth=valueStart;
 } break;
