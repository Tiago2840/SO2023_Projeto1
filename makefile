# Commands to build the code files
buildbase:
	gcc -o BaseVersion baseVersion.c

buildadvanced:
	gcc -o AdvancedVersion advancedVersion.c

buildoriginal:
	gcc -o OriginalVersion originalVersion.c

buildall: buildbase buildadvanced buildoriginal

# Commands to run a quick test
base:
	./BaseVersion ./testfiles/ex5.txt 10 1

advanced:
	./AdvancedVersion ./testfiles/ex5.txt 10 1

original:
	./OriginalVersion ./testfiles/ex5.txt 10 1

# Commands to build and run
execbase: buildbase base

execadvanced: buildadvanced advanced

execoriginal: buildoriginal original

execall: execbase execadvanced execoriginal

# Command to clean up the compiled files
clean:
	rm -f BaseVersion AdvancedVersion OriginalVersion