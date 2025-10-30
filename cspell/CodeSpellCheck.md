### run code spell check

```shell
sudo npm install -g cspell
cspell "{src,tests,docs}/**/*.{h,c,hpp,cpp,cc,ipp,md}"
```

### Add new words to code spell check

1. Open projectRoot/cspell.json
2. Add the new word to "words" field

### Use code spell check in vscode

<https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker>
