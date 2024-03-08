package main

import (
	"flag"
	"fmt"
	"github.com/tealeg/xlsx/v3"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"
)

var (
	excelDir = flag.String("excel", "./excel", "excel表目录")
	dataDir  = flag.String("data", "./data", "生成的数据文件目录")
	cppDir   = flag.String("cpp", "./cpp", "生成的cpp文件目录")
	protoc   = flag.String("protoc", "", "protoc工具的路径")
	oneProto = flag.Bool("oneproto", true, "所有proto结构输出到一个文件中")
)

var (
	allSheetDesc = []*sheetDesc{}
)

func main() {
	flag.Parse()

	files := findExcels(*excelDir)
	for _, file := range files {
		descs, err := parseFile(filepath.Join(*excelDir, file))
		if err != nil {
			return
		}

		allSheetDesc = append(allSheetDesc, descs...)
	}

	if genData(allSheetDesc) != nil {
		return
	}

	if genProto(allSheetDesc) != nil {
		return
	}

	if genCpp(allSheetDesc) != nil {
		return
	}

	if genReader(allSheetDesc) != nil {
		return
	}

	if genReaderVariable(allSheetDesc) != nil {
		return
	}

	for _, desc := range allSheetDesc {
		desc.sheet.Close()
	}
}

func findExcels(dir string) []string {
	fmt.Println("正在扫描excel目录:", dir)

	entries, err := os.ReadDir(dir)
	if err != nil {
		fmt.Println("打开excel表目录错误:", dir, err)
		return nil
	}

	files := []string{}
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		if ext := filepath.Ext(entry.Name()); ext != ".xlsx" {
			continue
		}
		files = append(files, entry.Name())
	}

	return files
}

// 生成proto文件
func genProto(descs []*sheetDesc) error {
	writeFile := func(protoname, content string) error {
		file, err := os.Create(protoname)
		if err != nil {
			fmt.Println("生成proto文件失败:", protoname, err)
			return nil
		}

		defer file.Close()
		file.WriteString("syntax = \"proto3\";\n")
		file.WriteString("package sheetcfg;\n\n")
		file.WriteString(content)
		return nil
	}

	writeProto := func(desc *sheetDesc) string {
		content := ""
		content += fmt.Sprintf("// 文件生成时间: %s\n", time.Now().String())
		content += fmt.Sprintf("// %s => [%s].sheet\n", desc.excelName, desc.sheetName)
		content += fmt.Sprintf("message %s {\n", desc.sheetName)
		for idx, field := range desc.desc {
			isRepeated := len(field.col) > 1
			content += "  "
			if isRepeated {
				content += "repeated "
			}

			if field.fieldType == "int" {
				field.fieldType = "int64"
			}

			content += fmt.Sprintf("%s %s = %d; // %s \n", field.fieldType, field.field, idx+1, field.comment)
		}
		content += "}\n"

		content += fmt.Sprintf("message %sSheet {\n", desc.sheetName)
		content += fmt.Sprintf("  repeated %s items = 1;\n", desc.sheetName)
		content += "}\n\n"
		return content
	}

	os.MkdirAll(*cppDir, os.ModeDir)

	if *oneProto {
		content := ""
		for _, desc := range descs {
			content += writeProto(desc)
		}
		if err := writeFile(filepath.Join(*cppDir, "sheet.proto"), content); err != nil {
			return err
		}
	} else {
		for _, desc := range descs {
			content := writeProto(desc)
			if err := writeFile(filepath.Join(*cppDir, desc.protoName), content); err != nil {
				return err
			}
		}
	}

	return nil
}

// 解析文件
func parseFile(filename string) ([]*sheetDesc, error) {
	file, err := xlsx.OpenFile(filename)
	if err != nil {
		fmt.Println("打开excel文件错误:", filename, err)
		return nil, err
	}

	descs := []*sheetDesc{}

	for _, sheet := range file.Sheets {
		dDesc := parseDataDesc(filename, sheet)
		if dDesc == nil {
			continue
		}

		desc := &sheetDesc{
			excelName:  filepath.Base(filename),
			sheetName:  sheet.Name,
			readerName: fmt.Sprintf("%sReader", sheet.Name),
			protoName:  fmt.Sprintf("%s.proto", sheet.Name),
			dataName:   fmt.Sprintf("%s.data", sheet.Name),
			desc:       dDesc,
			sheet:      sheet,
		}
		descs = append(descs, desc)
	}

	return descs, nil
}

// 解析表头描述
func parseDataDesc(filename string, sheet *xlsx.Sheet) []*dataDesc {
	if sheet.MaxCol <= 0 || sheet.MaxRow <= 0 {
		return nil
	}

	desc := []*dataDesc{}
	for ncol := 0; ncol < sheet.MaxCol; ncol++ {
		field := &dataDesc{}
		field.col = append(field.col, ncol)
		for nrow := 0; nrow < 4; nrow++ {
			cell, err := sheet.Cell(nrow, ncol)
			if err != nil {
				fmt.Print("打开[第%n行, 第%n列]错误，sheet:%s|文件:%s\n", nrow, ncol, sheet.Name, filename)
				return nil
			}

			// check
			if cell.Value == "#" {
				if nrow == 0 {
					break
				}
			}
			if cell.Value == "" {
				if nrow != 3 {
					fmt.Print("[第%n行, 第%n列]为空，sheet:%s|文件:%s\n", nrow, ncol, sheet.Name, filename)
					return nil
				}
			}

			if nrow == 0 {
				field.field = cell.Value
			} else if nrow == 1 {
				field.fieldType = strings.Split(cell.Value, "[]")[0]
			} else if nrow == 2 {
				field.comment = strings.ReplaceAll(cell.Value, "\r", " ")
				field.comment = strings.ReplaceAll(field.comment, "\n", " ")
			} else {
			}
		}

		if field.field == "" {
			continue
		}

		if idx := findDataDesc(desc, field.field); idx == -1 {
			desc = append(desc, field)
		} else {
			if desc[idx].fieldType != field.fieldType {
				fmt.Print("字段%s重复，sheet:%s|文件:%s\n", field.field, sheet.Name, filename)
				return nil
			}
			desc[idx].col = append(desc[idx].col, field.col...)
		}
	}

	return desc
}

// 解析表数据
func parseData(desc *sheetDesc) (string, error) {
	sheet := desc.sheet
	fields := desc.desc

	content := ""
	content += fmt.Sprintf("items: [\n")

	for nrow := 4; nrow < sheet.MaxRow; nrow++ {
		datas := map[string][]string{}
		for ncol := 0; ncol < sheet.MaxCol; ncol++ {
			idx := findDataDescByCol(fields, ncol)
			if idx == -1 {
				continue
			}

			cell, err := sheet.Cell(nrow, ncol)
			if err != nil {
				fmt.Print("打开[第%n行, 第%n列]错误，sheet:%s|文件:%s\n", nrow, ncol, sheet.Name, desc.excelName)
				return "", err
			}

			fieldname := fields[idx].field
			_, ok := datas[fieldname]
			if !ok {
				datas[fieldname] = []string{}
			}

			datas[fieldname] = append(datas[fieldname], cell.Value)
		}

		// check datas
		allempty := true
		for _, data := range datas {
			for _, value := range data {
				if len(value) != 0 {
					allempty = false
					break
				}
			}
			if allempty == false {
				break
			}
		}
		if allempty {
			continue
		}

		if nrow != 4 {
			content += ",\n"
		}

		content += "  {"
		for idx, field := range fields {
			if idx > 0 {
				content += ","
			}
			content += " " + field.field + ":"
			if len(field.col) > 1 {
				content += "["
			}
			for idx2, value := range datas[field.field] {
				if idx2 > 0 {
					content += ","
				}
				//fmt.Println(field.field, field.fieldType)
				if field.fieldType == "string" {
					content += fmt.Sprintf("\"%s\"", value)
				} else {
					if len(value) == 0 {
						content += "0"
					} else {
						content += value
					}
				}
			}
			if len(field.col) > 1 {
				content += "]"
			}
		}
		content += "}"
	}

	content += "\n]\n"
	return content, nil
}

// 生成数据文件
func genData(descs []*sheetDesc) error {
	writeFile := func(dataname, content string) error {
		file, err := os.Create(dataname)
		if err != nil {
			fmt.Println("生成data文件失败:", dataname, err)
			return err
		}

		defer file.Close()
		file.WriteString(content)
		return nil
	}

	writeData := func(desc *sheetDesc) (string, error) {
		content := ""
		content += fmt.Sprintf("# 文件生成时间: %s\n", time.Now().String())
		content += fmt.Sprintf("# %s => [%s].sheet\n", desc.excelName, desc.sheetName)
		c, err := parseData(desc)
		if err != nil {
			return "", err
		}
		content += c
		return content, nil
	}

	os.MkdirAll(*dataDir, os.ModeDir)

	for _, desc := range descs {
		content, err := writeData(desc)
		if err != nil {
			return err
		}

		if err = writeFile(filepath.Join(*dataDir, desc.dataName), content); err != nil {
			return err
		}
	}

	return nil
}

// 生成cpp文件
func genCpp(descs []*sheetDesc) error {
	protocExe := *protoc
	if len(protocExe) == 0 {
		if runtime.GOOS == "windows" {
			protocExe = "protoc.exe"
		} else {
			protocExe = "protoc"
		}
	}

	writeCpp := func(name string) error {
		cmd := exec.Command(protocExe, "--cpp_out=./", name)
		if out, err := cmd.CombinedOutput(); err != nil {
			fmt.Println("生成cpp文件失败:", name)
			fmt.Println(string(out))
			return err
		}
		return nil
	}

	if *oneProto {
		return writeCpp(filepath.Join(*cppDir, "sheet.proto"))
	} else {
		for _, desc := range descs {
			if err := writeCpp(filepath.Join(*cppDir, desc.protoName)); err != nil {
				return err
			}
		}
	}

	return nil
}

// 生成sheet_reader文件
func genReader(descs []*sheetDesc) error {
	writeFile := func(readername string, content string) error {
		file, err := os.Create(readername)
		if err != nil {
			fmt.Println("生成reader文件失败:", readername, err)
			return err
		}

		defer file.Close()
		file.WriteString(content)
		return nil
	}

	writeReader := func(desc *sheetDesc) (string, error) {
		content := ""
		content += fmt.Sprintf("// 文件首次生成于时间: %s \n", time.Now().String())
		content += "#pragma once\n\n"
		content += "#include \"sheet_reader.h\"\n"

		if *oneProto {
			content += fmt.Sprintf("#include \"%s.pb.h\"\n\n", "sheet")
		} else {
			content += fmt.Sprintf("#include \"%s.pb.h\"\n\n", desc.sheetName)
		}

		content += "namespace sheetcfg {\n"
		content += "    // 索引类型\n"
		content += fmt.Sprintf("    using %sKey=SheetKey<>;\n", desc.sheetName)
		content += fmt.Sprintf("    // 模板参数分别为：索引类型，自定义结构类型，默认类型(proto生成), proto表类型\n")
		content += fmt.Sprintf("    using %sBaseReader=SheetReader<%s, %s, %s, %s>;\n\n",
			desc.sheetName,
			desc.sheetName+"Key",
			desc.sheetName,
			desc.sheetName,
			desc.sheetName+"Sheet")
		content += fmt.Sprintf("    struct %s : public %sBaseReader {\n", desc.readerName, desc.sheetName)
		content += "    protected:\n"
		content += "        //解析器实现\n"
		content += fmt.Sprintf("        bool parser(%s& key, %s& newitem, %s& item) {\n",
			desc.sheetName+"Key",
			desc.sheetName,
			desc.sheetName)
		content += "            newitem = item;\n"
		content += "            key.key1 = newitem.id();\n"
		content += "            return true;\n"
		content += "        }\n"
		content += "    };\n"
		content += "}"
		return content, nil
	}

	for _, desc := range descs {
		content, err := writeReader(desc)
		if err != nil {
			return err
		}
		err = writeFile(filepath.Join(*cppDir, desc.readerName+".h"), content)
		if err != nil {
			return err
		}
	}
	return nil
}

// 生成reader变量
func genReaderVariable(descs []*sheetDesc) error {
	headers := "#pragma once\n\n"
	cpps := ""

	for _, desc := range descs {
		headers += fmt.Sprintf("#include \"%s.h\"\n", desc.readerName)
	}

	headers += "\nnamespace sheetcfg {\n"
	cpps += "#include \"AllSheetReader.h\"\n\n"
	cpps += "namespace sheetcfg {\n"

	for _, desc := range descs {
		headers += fmt.Sprintf("    extern %s g%s;\n", desc.readerName, desc.readerName)
		cpps += fmt.Sprintf("    %s g%s;\n", desc.readerName, desc.readerName)
	}

	writeFile := func(filename, content string) error {
		file, err := os.Create(filename)
		if err != nil {
			fmt.Print("生成%s文件错误: %v\n", filename, err)
			return err
		}

		defer file.Close()
		file.WriteString(fmt.Sprintf("// 文件生成时间: %s\n\n", time.Now().String()))
		file.WriteString(content)
		return nil
	}

	headers += "    bool LoadAllReader(const std::string&);\n"
	headers += "}\n"

	cpps += "\n"
	cpps += "    bool LoadAllReader(const std::string& dir) {\n"
	for _, desc := range descs {
		dataname := fmt.Sprintf("(dir+\"/%s\").c_str()", desc.dataName)
		cpps += fmt.Sprintf("        if (%s.Load(%s) == false) return false;\n", "g"+desc.readerName, dataname)
	}
	cpps += "        return true;\n"
	cpps += "    }\n"
	cpps += "}"

	if err := writeFile(filepath.Join(*cppDir, "AllSheetReader.h"), headers); err != nil {
		return err
	}

	if err := writeFile(filepath.Join(*cppDir, "AllSheetReader.cpp"), cpps); err != nil {
		return err
	}

	return nil
}

type dataDesc struct {
	field     string // 字段名
	fieldType string // 字段类型
	comment   string // 注释
	col       []int  // 列标号
}

func findDataDesc(fields []*dataDesc, field string) int {
	for idx, f := range fields {
		if f.field == field {
			return idx
		}
	}
	return -1
}

func findDataDescByCol(fields []*dataDesc, col int) int {
	for idx, f := range fields {
		for _, c := range f.col {
			if c == col {
				return idx
			}
		}
	}
	return -1
}

type sheetDesc struct {
	excelName  string      // excel表名字
	sheetName  string      // sheet表名字
	readerName string      // reader名字
	protoName  string      // proto文件名字
	dataName   string      // data文件名字
	desc       []*dataDesc // 表头描述
	sheet      *xlsx.Sheet // 表
}
