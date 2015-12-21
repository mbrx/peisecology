#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <sstream>

#include <signal.h>

#include "util.h"
#include "regex.h"
#include "evaluate.h"

extern "C" {
	#include <peiskernel/peiskernel_mt.h>
	#include <peiskernel/peiskernel.h>
}


#include "./tinyxml2/tinyxml2.h"

#include "./muparserx/parser/mpDefines.h"

#include "xml_format_readme.h"


const char* STR_DO_QUIT_TUPLE_NAME = "do-quit";

const char* XML_ELEM_RULES = "rules";
const char* XML_ELEM_MATCH = "match";
const char* XML_ELEM_REWRITE = "rewrite";
const char* XML_ELEM_EVAL = "eval";

const char* XML_ATTR_KEY = "key";
const char* XML_ATTR_REPL = "repl";
const char* XML_ATTR_EXPR = "expr";
const char* XML_ATTR_TYPE = "type";

//Definition of data type strings for the eval type argument
const DataType DEFAULT_DATA_TYPE = DT_DOUBLE;
const char* DT_STR_DOUBLE = "double";
const char* DT_STR_STRING = "string";
const char* DT_STR_INTEGER = "integer";
const char* DT_STR_BOOLEAN = "boolean";

const RegExp REGEXP_IS_DETERMINISTIC_KEY("[0-9a-zA-Z]+");
const RegExp REGEXP_IS_WILDCARD_KEY("\\*");

tinyxml2::XMLDocument doc;

std::set<PeisSubscriberHandle> subscriberHandles;
std::set<PeisCallbackHandle> callbackHandles;

// Forward declarations //////////////////
int main(int argc, const char* argv[]);
bool matches(const std::string& key, const std::string& templ);
void tupleCallback(PeisTuple *tuple, void *userdata);
void resubscribe();

bool do_quit = false;

bool onlyNew = false;


bool peisk_insertNewTuple(PeisTuple* newTuple) {
	
	char keybuffer[1024];
	peisk_getTupleName(newTuple, keybuffer, sizeof(keybuffer));
	
	PeisTuple* pOldTuple = peisk_getTuple(newTuple->owner, keybuffer, PEISK_KEEP_OLD | PEISK_NON_BLOCKING);
	
	if(pOldTuple != 0 && pOldTuple->datalen == newTuple->datalen && std::memcmp(newTuple->data, pOldTuple->data, newTuple->datalen) == 0) {
		return false;
	} else {
		peisk_insertTuple(newTuple);
		return true;
	}
}

// Implementation ////////////////////////
int main(int argc, const char* argv[]) {
	
	std::cout << "Version 0.0.3" << std::endl;
	std::cout << "MuparserX version: " << MUP_PARSER_VERSION << std::endl;
	std::cout << "TinyXML version: " << TIXML2_MAJOR_VERSION << "." << TIXML2_MINOR_VERSION << "." << TIXML2_PATCH_VERSION << std::endl;
	std::cout << "TReX version: " << "?" << std::endl;

	std::string fileName = "xbees.xml";
	
	//Evaluate the command line arguments
	{
		if(argc <= 1 || std::string(argv[1]) == "--help") {
			//TODO: Document the --onlynew option
			std::cout << "Usage " << "peistrans" << "{--onlynew} {peis options} [xml file]" << std::endl;
			std::cout << std::string(reinterpret_cast<const char*>(xml_format_readme_txt), 0, xml_format_readme_txt_len);
			return EXIT_SUCCESS;
		} 
		
		for(int i = 0; i < argc; ++i) {
			std::string arg = argv[i];
			if(arg == "--onlynew") {
				std::cout << "Only setting new tuples" << std::endl;
				onlyNew = true;
			}
		}
		
		fileName = argv[argc-1];
	}
	
	//Initialize the PEIS kernel
	peiskmt_initialize(&argc, const_cast<char**>(argv));
	peiskmt_setStringTuple(const_cast<char*>(STR_DO_QUIT_TUPLE_NAME), "");
	
	//Load the XML document and subscribe to necessary tuples
	if(!doc.LoadFile(fileName.c_str()) == tinyxml2::XML_NO_ERROR) {
		std::cerr << "Failed to load file " << fileName << std::endl;
		std::cerr << ": " << doc.GetErrorStr1() << std::endl;
		std::cerr << doc.GetErrorStr2() << std::endl;
		return EXIT_FAILURE;
	}
	
	resubscribe();
	
	//Handle SIGINT by closing the listening socket
	struct LocalFunctionWrapper {
		static void siginthandler(int param) {
			do_quit = true;
		}
	};
	//Register the signal handler
	signal(SIGINT, LocalFunctionWrapper::siginthandler);
	
	//Wait here until finished
	for(; !do_quit;) {
		peiskmt_lock();
		PeisTuple* peisTuple = peisk_getTuple(peisk_peisid(), const_cast<char*>(STR_DO_QUIT_TUPLE_NAME), PEISK_KEEP_OLD | PEISK_NON_BLOCKING);
		do_quit = std::string(peisTuple->data).size() > 0;
		peiskmt_unlock();
		sleep(1);
	}
	
	/*
	peiskmt_lock();
	
	for(std::set<PeisCallbackHandle>::const_iterator i = callbackHandles.begin(); i != callbackHandles.end(); ++i) {
		peisk_unsubscribe(*i);
	}
	
	for(std::set<PeisSubscriberHandle>::const_iterator i = subscriberHandles.begin(); i != subscriberHandles.end(); ++i) {
		peisk_unregisterTupleCallback(*i);
	}
	
	peiskmt_unlock();
	*/
	
	peiskmt_shutdown();
	
	return EXIT_SUCCESS;
}


bool matches(const std::string& key, const std::string& templ) {
	try {
		return REGEXP_IS_WILDCARD_KEY.Match(templ) || RegExp(templ).Match(key);
	} catch(...) {
		std::cerr << "Error while matching" << key << " with " << templ << std::endl;
	}
	return false;
}

void tupleCallback(PeisTuple *tuple, void *userdata) {
	using namespace tinyxml2;
	
	//Only handle each tuple once and prevent infinite loops
	if(tuple->isNew == 0 || tuple->owner == peisk_peisid()) {
		return;
	}
	
	//Set it to prevent infinite loops
	tuple->isNew = 0;
	
	//Get the name of the tuple
	std::vector<std::string> keyParts;
	{
		std::vector<char> buff(PEISK_KEYLENGTH);
		peisk_getTupleName(tuple, &buff[0], buff.size());
		splitstr(std::string(&buff[0]), '.', keyParts);
	}
	
	std::vector<std::string> keyPartsOut(keyParts.size());
	
	struct recursion_wrapper {
		
		static void process(unsigned lvl, const std::vector<std::string>& keyParts, std::vector<std::string>& keyPartsOut, const XMLElement* element, PeisTuple* tuple) {
			
			if(lvl >= keyParts.size()) {
				return;
			}
			
			for(const XMLElement* pMatch = element->FirstChildElement(XML_ELEM_MATCH); pMatch != 0; pMatch = pMatch->NextSiblingElement(XML_ELEM_MATCH) ) {
				const std::string templ = pMatch->Attribute(XML_ATTR_KEY);
				
				if(matches(keyParts[lvl], templ)) {
					
					keyPartsOut[lvl] = keyParts[lvl];
					
					//Find and process all rewrite rules at this level
					for(const XMLElement* pRewrite = pMatch->FirstChildElement(XML_ELEM_REWRITE); pRewrite != 0; pRewrite = pRewrite->NextSiblingElement(XML_ELEM_REWRITE) ) {
						const std::string templ = pRewrite->Attribute(XML_ATTR_KEY);
						const std::string repl = pRewrite->Attribute(XML_ATTR_REPL);
						
						if(matches(keyPartsOut[lvl], templ)) {
//							std::clog << std::string(lvl, '\t') << keyPartsOut[lvl] << " --> " << repl << std::endl;
							keyPartsOut[lvl] = repl;
						}
					}
					
					//At this point we should have a fully rewritten key
					std::string originalLevelKey = concatstr(&keyParts[0], &keyParts[lvl+1], '.');;
					
					//If we have rewritten something to "", then rewrittenLevelKey will contain a "..", 
					//Thus we remove empty subkeys
					std::string rewrittenLevelKey = concatstrI(&keyPartsOut[0], &keyPartsOut[lvl+1], '.', std::string());
					
					//Lets evaluate and set the key if it is not empty
					const PeisTuple* remoteSubkeyTuple = peisk_getTuple(tuple->owner, originalLevelKey.c_str(), PEISK_KEEP_OLD);
					
					//If the mirroring rule is at the correct level (hack)
					//and if this tuple really exists, e.g. if the tuple a.b.c exists, a.b does not necessarily do so
					if(lvl == (keyParts.size() - 1) && remoteSubkeyTuple != 0)  {
						
						
						//"Clone" the input tuple so that we keep its expiration time etc.
						PeisTuple newLocalTuple = *remoteSubkeyTuple;
						newLocalTuple.owner = peisk_peisid(); //Set to "this"
						
						peisk_setTupleName(&newLocalTuple, rewrittenLevelKey.c_str());
						
						const XMLElement* evalElement = pMatch->FirstChildElement(XML_ELEM_EVAL);
						if(evalElement != 0) {
							
							//Get the data type
							DataType dataType = DEFAULT_DATA_TYPE;
							if(evalElement->Attribute(XML_ATTR_TYPE) != 0) {
								std::string type = evalElement->Attribute(XML_ATTR_TYPE);
								
								if(type == DT_STR_DOUBLE) {
									dataType = DT_DOUBLE;
								} else if(type == DT_STR_STRING) {
									dataType = DT_STRING;
								} else if(type == DT_STR_INTEGER) {
									dataType = DT_INTEGER;
								} else if(type == DT_STR_BOOLEAN) {
									dataType = DT_BOOLEAN;
								} else {
									std::cerr << "Unknown data type: " << type << std::endl;
								}
							}
							
							//Evaluate the value using the provided function
							std::string expr = evalElement->Attribute(XML_ATTR_EXPR);
							std::string originalValue = remoteSubkeyTuple->data;
							std::string nevValue = evaluate(originalValue, expr, dataType);
							
							newLocalTuple.data = const_cast<char*>(nevValue.c_str());
							newLocalTuple.datalen = nevValue.size() + 1;
							
							std::clog << std::string(lvl, '\t') << "Rewriting " 
								<< originalLevelKey << "{" << originalValue << "}"
								<< " --> " 
								<< rewrittenLevelKey << "{" << nevValue << "}"
								<< " (" << expr << ")" << std::endl;
								
							if(onlyNew) {
								newLocalTuple.ts_expire[0] = 0; //Never expire
								newLocalTuple.ts_expire[1] = 0;
								peisk_insertNewTuple(&newLocalTuple);
							} else {
								peisk_insertTuple(&newLocalTuple);
							}
						} else {
							std::clog << std::string(lvl, '\t') << "Mirroring " << originalLevelKey << " --> " << rewrittenLevelKey << std::endl;
							newLocalTuple.data = remoteSubkeyTuple->data;
							newLocalTuple.datalen = remoteSubkeyTuple->datalen;
							
							if(onlyNew) {
								newLocalTuple.ts_expire[0] = 0; //Never expire
								newLocalTuple.ts_expire[1] = 0;
								peisk_insertNewTuple(&newLocalTuple);
							} else {
								peisk_insertTuple(&newLocalTuple);
							}
						}
						
					}
					
					//Recursively process the keys
					process(lvl+1, keyParts, keyPartsOut, pMatch, tuple);
				}
			}
			
		}
	};
	
	const XMLElement* pMatch = doc.FirstChildElement(XML_ELEM_RULES);
	
	recursion_wrapper::process(0, keyParts, keyPartsOut, pMatch, tuple);
}

void resubscribe() {
	using namespace tinyxml2;
	
	const XMLElement* ruleElement = doc.FirstChildElement(XML_ELEM_RULES);
	
	struct recursion_wrapper {
		static void process(std::set<std::string>& keys, const XMLElement* element, std::vector<std::string> parent = std::vector<std::string>()) {
			
			for(const XMLElement* pMatch = element->FirstChildElement(XML_ELEM_MATCH); pMatch != 0; pMatch = pMatch->NextSiblingElement(XML_ELEM_MATCH) ) {
				std::vector<std::string> current = parent;
				
				std::string subKey;
				{
					const char* pKey = pMatch->Attribute(XML_ATTR_KEY);
					if(pKey == 0) {
						pKey = "*";
					}
					subKey = pKey;
				}
				
				if(REGEXP_IS_DETERMINISTIC_KEY.Match(subKey)) {
					//std::cout << "DET: " << "\"" << subKey << "\"" << std::endl;
					current.push_back(subKey);
				} else {
					//std::cout << "WLD: " << "\"" << subKey << "\"" << std::endl;
					current.push_back("*");
				}
				
				//std::cout << current << std::endl;
				keys.insert( concatstr(current.begin(), current.end(), '.'));
				
				process(keys, pMatch, current);
			}
		}
	};
	
	std::set<std::string> keysToSubscribeTo;
	recursion_wrapper::process(keysToSubscribeTo, ruleElement);
	
	/*
	//Remove less general subscription keys (Optional)
	{
		std::vector<std::string> keyVec(keysToSubscribeTo.begin(), keysToSubscribeTo.end());
		
		for(int i = 0; i < keyVec.size(); ++i) {
			PeisTuple I;
			peisk_setTupleName(&I, keyVec[i].c_str());
			
			for(int j = 0; j < keyVec.size(); ++j) {
				PeisTuple J;
				peisk_setTupleName(&J, keyVec[j].c_str());
				
				if(i != j && peisk_isGeneralization(&I, &J)) {
					keyVec[j] = "";
				}
				std::cout << keyVec[i] << "~" << keyVec[j] << ": " << peisk_isGeneralization(&I, &J) << std::endl;
			}
		}
		
		keysToSubscribeTo = std::set<std::string>(keyVec.begin(), keyVec.end());
		keysToSubscribeTo.erase("");
	}
	*/
	
	for(std::set<std::string>::const_iterator i = keysToSubscribeTo.begin(); i != keysToSubscribeTo.end(); ++i) {
		const std::string& toSubscribeKey = *i;
		std::cout << "Subscribing to " << toSubscribeKey << std::endl;
		peiskmt_lock();
		
		PeisSubscriberHandle subscriberHandle = peisk_subscribe(-1, toSubscribeKey.c_str());
		PeisCallbackHandle callbackHandle = peisk_registerTupleCallback(-1, toSubscribeKey.c_str(), 0, tupleCallback);
		
		subscriberHandles.insert(subscriberHandle);
		callbackHandles.insert(callbackHandle);
		
		peiskmt_unlock();
	}
	
	// Put the xml document in tuplespace for reference
	{
		peiskmt_lock();
		
		XMLPrinter xmlPrinter;
		doc.Print(&xmlPrinter);
		peisk_setStringTuple("config-readonly", xmlPrinter.CStr());

		peiskmt_unlock();
	}
	
}







