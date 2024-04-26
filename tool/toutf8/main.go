package main

import (
	"bytes"
	"flag"
	"fmt"
	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"time"
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

	fmt.Println("over, waiting close")
	time.Sleep(time.Second * 10)
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

		isBom := isUTF8BOM(data)
		isUtf8 := utf8.Valid(data)
		isGbk := isGBK(data)

		if isBom {
			if isUtf8 {
				convert2("bom", data, file)
			} else {
				fmt.Println("无法准确识别文件的编码:", file)
			}
		} else if isUtf8 && !isGbk {
			convert2("utf8", data, file)
		} else if isGbk && !isUtf8 {
			convert2("gbk", data, file)
		} else {
			fmt.Println("无法准确识别文件的编码:", file)
		}
	}
	return true
}

func isUTF8BOM(data []byte) bool {
	if len(data) < 3 {
		return false
	}
	// UTF-8 BOM的字节序列
	return data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF
}

func isGBK(data []byte) bool {
	dataLen := len(data)
	var i int = 0
	for i < dataLen {
		if data[i] <= 0x7f {
			//编码0~127,只有一个字节的编码，兼容ASCII码
			i++
			continue
		} else {
			if i+1 >= dataLen {
				return false
			}
			//大于127的使用双字节编码，落在gbk编码范围内的字符
			if data[i] >= 0x81 &&
				data[i] <= 0xfe &&
				data[i+1] >= 0x40 &&
				data[i+1] <= 0xfe &&
				data[i+1] != 0xf7 {
				i += 2
				continue
			} else {
				return false
			}
		}
	}
	return true
}

func newOutFilePath(file string) string {
	if len(*out) == 0 {
		return file
	}

	dirs := strings.Split(file, string(filepath.Separator))
	fPath := *out
	dir := *out
	idx := 0
	if len(dirs) > 1 {
		idx = 1
	}

	for ; idx < len(dirs); idx++ {
		fPath = filepath.Join(fPath, dirs[idx])
		if idx != len(dirs)-1 {
			dir = filepath.Join(dir, dirs[idx])
		}
	}

	os.MkdirAll(dir, os.ModeDir)
	return fPath
}

func convert2(srcCharset string, data []byte, file string) {
	fPath := newOutFilePath(file)
	//fmt.Println(fPath)

	if *withBom && srcCharset == "bom" {
		if fPath == file {
			return
		}
	}
	if *withBom == false && srcCharset == "utf8" {
		if fPath == file {
			return
		}
	}

	output, err := os.Create(fPath) //os.OpenFile(fPath, os.O_WRONLY|os.O_TRUNC|os.O_CREATE, 0644)
	if err != nil {
		fmt.Println(err)
		return
	}

	defer output.Close()

	if *withBom {
		if srcCharset == "bom" {
			_, err = io.Copy(output, bytes.NewReader(data))
			if err != nil {
				fmt.Println(err)
			}
		} else if srcCharset == "utf8" {
			output.Write([]byte{0xEF, 0xBB, 0xBF})
			_, err = io.Copy(output, bytes.NewReader(data))
			if err != nil {
				fmt.Println(err)
			}
		} else if srcCharset == "gbk" {
			output.Write([]byte{0xEF, 0xBB, 0xBF})
			reader := transform.NewReader(bytes.NewBuffer(data), simplifiedchinese.GBK.NewDecoder())
			_, err = io.Copy(output, reader)
			if err != nil {
				fmt.Println(err)
			}
		}
	} else {
		if srcCharset == "utf8" {
			_, err = io.Copy(output, bytes.NewReader(data))
			if err != nil {
				fmt.Println(err)
			}
		} else if srcCharset == "bom" {
			_, err = io.Copy(output, bytes.NewReader(data[3:]))
			if err != nil {
				fmt.Println(err)
			}
		} else if srcCharset == "gbk" {
			reader := transform.NewReader(bytes.NewBuffer(data), simplifiedchinese.GBK.NewDecoder())
			_, err = io.Copy(output, reader)
			if err != nil {
				fmt.Println(err)
			}
		}
	}

	fmt.Println("成功转换的文件:", file)
}
