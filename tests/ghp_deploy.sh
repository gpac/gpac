#!/bin/bash

if [ -z "$GH_TOKEN" ]; then
    echo 'GH_TOKEN is not defined, do nothing'
    exit 0
fi

set -e # exit with nonzero exit code if anything fails

cp -rf stylesheet.xsl index.html ../coverage ../coverage.info results

# go to the results directory and create a *new* Git repo
cd results
rm -rf .git
git init

# inside this git repo we'll pretend to be a new user
git config user.name "Travis CI"
git config user.email "travisci@gpac.io"

# The first and only commit to this new Git repo contains all the
# files present with the commit message "Deploy to GitHub Pages".
git add all_results.xml stylesheet.xsl index.html logs coverage coverage.info
git commit -q -m "Deploy to GitHub Pages"

# Force push from the current repo's master branch to the remote
# repo's gh-pages branch. (All previous history on the gh-pages branch
# will be lost, since we are overwriting it.) We redirect any output to
if grep "platform=\"Linux\"" all_results.xml > /dev/null; then
    if [ -z "$GH_REF_LINUX" ]; then
        echo 'GH_REF_LINUX is not defined, do nothing'
        exit 0
    fi
    git push --force --quiet "https://${GH_TOKEN}@${GH_REF_LINUX}" master:gh-pages > /dev/null 2>&1
elif grep "platform=\"Darwin\"" all_results.xml > /dev/null; then
   if [ -z "$GH_REF_MACOS" ]; then
        echo 'GH_REF_MACOS is not defined, do nothing'
        exit 0
    fi
    git push --force --quiet "https://${GH_TOKEN}@${GH_REF_MACOS}" master:gh-pages > /dev/null 2>&1 
else
    echo 'platform unknown, do nothing'
fi
