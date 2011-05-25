
Element["" "Combined Molex 156 and AKL 220 Connector, 4 Pin" "" "" 10000 10000 0 0 0 100 ""]
(
# Molex 156
	Pin[ -7874 0 12800 2000 13300 7000 "" "1" "edge2"]
	Pin[  7874 0 12800 2000 13300 7000 "" "2" "edge2"]
	ElementLine [-15748 -21600  15748 -21600 1000]
#	ElementLine [-15748  10000  15748  10000 1000]
	ElementLine [-15748 -21600 -15748  10000 1000]
	ElementLine [ 15748 -21600  15748  10000 1000]
	# space for plugging the connector
	ElementLine [-15748  21600 -10748  21600 1000]
	ElementLine [ 10748  21600  15748  21600 1000]
	ElementLine [-15748  21600 -15748  16600 1000]
	ElementLine [ 15748  21600  15748  16600 1000]
# AKL 220 / screw terminal
	Pin[-10000 -7500 12800 2000 13300 6000 "" "1" "edge2"]
	Pin[ 10000 -7500 12800 2000 13300 6000 "" "2" "edge2"]
	ElementLine [-22300 -22850  22300 -22850 1000]
	ElementLine [-22300  10000  22300  10000 1000]
	ElementLine [-22300  10000 -22300 -22850 1000]
	ElementLine [ 22300  10000  22300 -22850 1000]
	ElementLine [-22300   7850 -18300   7850 1000]
	ElementLine [ 18300   7850  22300   7850 1000]
)
