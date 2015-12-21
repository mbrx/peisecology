### Usage ###

peistrans {--onlynew} [xml file] {peis options}

--onlynew   Prevents the output tuples from being updated if their data has not changed.
            Using this option alters the ts_expire timestamp on translated tuples
            to be far into the future.


### Elements ###

<rules>
	Is the root element of the XML file, containing one or more <match> elements

<match key="xbees"> 
	Matches a key, ? can be either a regular expression, e.g. "a?[1-2]+" or, a wildcard "*"
	(Fully qualified keys, e.g. "sensors" are also regular expressions)

<rewrite key="ad0" repl="luminosity" />
	Replaces the current key, the key attribute is a wildcard or a regex,
	the repl attribute should be a valid part of a tuple key.
	Also, the rewrite arguments are processed in order of occurence,
	that is, the _replacement_ of a rewrite is further evaluated. Ex.:
	
	<match key="a">                     Replacement key is a
		<rewrite key="a" repl="b" />  Replacement key is b
		<rewrite key="a" repl="c" />  Replacement key is b
		<rewrite key="b" repl="d" />  Replacement key is d
	</match>

<eval expr="x*x" type="double">
	If the current key exists in the tuplespace the 
	generated value will be the evaluation of the expression, 
	x denotes the original value.
	The default datatype of x is double, to use another datatype add
	the attribute type. Current supported types are 
	"double", "integer", "string" and "boolean". The input values are
	converted using corresponding c++ stream operations.

An XML file can also be validated against a schema using the xmllint tool:
xmllint --noout --schema schema.xsd  {your_xml_file.xml}


### XML Example ###
<?xml version="1.0"?>
<rules>
	<match key="xbees">
		
		<match key="*">
			<rewrite key="0" repl="kitchen" />
			
			<match key="ad0">
				<rewrite key="*" repl="luminosity" />
				<!-- <eval expr="x*x" /> -->
			</match>
			
			<match key="ad1">
				<rewrite key="*" repl="gap" />
			</match>
			
			<match key="ad2">
				<rewrite key="*" repl="infrared" />
			</match>
			
		</match>
		
	</match>
	
</rules>
