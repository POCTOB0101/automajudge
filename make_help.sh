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
if ! [[ -n "$RUNCALLED" ]]; then
    # means this was called by make and not run.sh
    # echo "a script didn't call me"
    rm files.c # cleanup
    unset FILECOUNT
fi
