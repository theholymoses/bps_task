#! /bin/bash

RED="\e[1;31m"
GREEN="\e[1;32m"
WHITE="\e[0m"

echo -e "${GREEN}Running make...${WHITE}"

make > /dev/null

if [ $? -ne 0 ]; then
    echo -e "${RED}Error occured while compiling${WHITE}"
    exit
fi

cd test

for file in *.sc; do
    echo -e "${GREEN}Running script $file...${WHITE}"
    ./sotest "$file"
done

cd - > /dev/null
