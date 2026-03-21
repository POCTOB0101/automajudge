ls *.c | sed G | grep eff > files.c
ls *.c | sed G | grep func >> files.c
# ^ get all the test files
FILECOUNT=$(wc -l < files.c)
# and how many there are
for ((i=1; i<=FILECOUNT; i++)); do
    FILE=$(awk -v i="$i" 'NR == i' files.c)
    # get names one at a time and compile them
    ($CC $FLAGS $DEBUG "$FILE" -o "$(basename -s .c "$FILE").exe")
done
rm files.c # cleanup
