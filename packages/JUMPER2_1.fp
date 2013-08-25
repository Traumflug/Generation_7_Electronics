Element["" "Jumper, i.e. single row headers" "" "JUMPER2" 160 0 3 100 0 100 0]
(
	Pin[5000  5000 8000 2000 8500 4000 "" "1" "edge2"]
	Pin[5000 15000 8000 2000 8500 4000 "" "2" "edge2"]
	ElementLine[0 0 0 20000 1000]
	ElementLine[0 20000 10000 20000 1000]
	ElementLine[10000 20000 10000 0 1000]
	ElementLine[10000 0 0 0 1000]
	ElementLine[0 10000 10000 10000 1000]

)
# Note there's a bug in the very popular 2009 version of PCB,
# which disallows the usage of dashs (-) in some footprint file names.
