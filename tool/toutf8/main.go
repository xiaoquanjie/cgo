package main

import (
	"bytes"
	"flag"
	"fmt"
	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
	"io/fs"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"unicode/utf8"
)

var (
	withBom   = flag.Bool("bom", false, "convert to utf8 with bom")
	out       = flag.String("out", "", "output directory")
	file      = flag.String("file", "", "input file or director")
	extention = flag.String("ext", "", "file name extension .eg txt|cpp|h|cc|c")
	extList   = []string{}
)

func main() {
	flag.Parse()

	if *file == "" {
		fmt.Println("no input file param")
		return
	}

	_, err := os.Stat(*file)
	if err != nil {
		nErr, ok := err.(*fs.PathError)
		if ok {
			fmt.Println(nErr.Err.Error(), ":", *file)
		} else {
			fmt.Println(err)
		}
		return
	}

	if *extention != "" {
		extList = strings.Split(*extention, "|")
	}

	traverse(*file)
}

func traverse(file string) {
	fInfo, err := os.Stat(file)
	if err != nil {
		fmt.Println(err)
		return
	}

	if fInfo.IsDir() {
		entries, err := os.ReadDir(file)
		if err != nil {
			fmt.Println(err)
		} else {
			for _, entry := range entries {
				traverse(filepath.Join(file, entry.Name()))
			}
		}
	} else {
		convert(file)
	}
}

func convert(file string) bool {
	yes := false
	if len(extList) != 0 {
		ext := filepath.Ext(file)
		for _, e := range extList {
			if e == ext {
				yes = true
				break
			}
		}
	} else {
		yes = true
	}

	if yes {
		data, err := os.ReadFile(file)
		if err != nil {
			fmt.Println(err)
			return false
		}
		if isUTF8BOM(data) {
			convert2("bom", data, file)
		} else if utf8.Valid(data) {
			convert2("utf8", data, file)
		} else {
			convert2("gbk", data, file)
		}
	}
	return true
}

func isUTF8BOM(data []byte) bool {
	// UTF-8 BOM的字节序列
	return data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF
}

func convert2(srcCharset string, data []byte, file string) {
	if *withBom {
		if srcCharset == "bom" {
		} else if srcCharset == "utf8" {

		} else if srcCharset == "gbk" {

		}
	} else {
		if srcCharset == "utf8" {
		} else if srcCharset == "bom" {

		} else if srcCharset == "gbk" {
			reader := transform.NewReader(bytes.NewReader(data), simplifiedchinese.GBK.NewDecoder())

			d, e := ioutil.ReadAll(reader)
			if e != nil {
				return nil, e
			}
			return d, nil
		}
	}

	fmt.Println(*withBom)
}
