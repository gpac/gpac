#!/bin/bash

# built from https://stackoverflow.com/a/76815733

if [ -z "$2" ]
then
    echo "Usage: $0 <original_file> <duplicated_file>"
    exit 0
fi

if [ !  -e "$1" ]
then
    echo "File $1 does not exist"
    exit 1
fi

if [ -n "$(git status --porcelain --untracked-files=no)" ]
then
    echo "You have uncommitted changes. Pleae only run this script on a clean local repo."
    exit 1
fi

originalFileLoc=$1
newFileLoc=$2

branchName=chore/temp/duplicate-file-history-by-script
currentBranchName="$(git branch --show-current)"

function copy_git_history() {
    targetToCopy=$1
    newDestination=$2

    echo
    echo "-- copying $targetToCopy to $newDestination and keeping its history --"
    echo

    git mv "$targetToCopy" "$newDestination"
    git commit -m "duplicating $targetToCopy to $newDestination"

    git checkout HEAD~ "$targetToCopy"
    git commit -m "restoring moved file $targetToCopy to its original location"
}

echo
echo "---- git copying $originalFileLoc to $newFileLoc ----"
echo

# create the new branch to store the changes
git checkout -b $branchName

# create the duplicate file(s)
if [ -d  "$originalFileLoc" ]
then
    files="$originalFileLoc/*"
    mkdir -p "$newFileLoc"

    for file in $files
    do
      copy_git_history "$file" "$newFileLoc"
    done
else
  copy_git_history "$originalFileLoc" "$newFileLoc"
fi

# switch back to source branch
git checkout -
# merge the history back into the source branch to retain both copies
git merge --no-ff $branchName -m "Merging file history for copying $originalFileLoc to $newFileLoc"

# delete the branch we created for history tracking purposes
git branch -D $branchName

echo
echo "---- all done ----"
echo
