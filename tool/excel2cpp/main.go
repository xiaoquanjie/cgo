package main

import (
	"flag"
	"fmt"
	"github.com/tealeg/xlsx/v3"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
)

var (
	excelDir   = flag.String("excel", "./excel", "excel表目录")
	dataDir    = flag.String("data", "./data", "生成的数据文件目录")
	cppDir     = flag.String("cpp", "./cpp", "生成的cpp文件目录")
	protoc     = flag.String("protoc", "", "protoc工具的路径")
	allReaders = []string{}
)

func main() {
	flag.Parse()

	files := findExcels(*excelDir)
	for _, f := range files {
		if err := genFile(filepath.Join(*excelDir, f)); err != nil {
			return
		}
	}

	genReaderVariable(allReaders)
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

// 生成文件
func genFile(filename string) error {
	file, err := xlsx.OpenFile(filename)
	if err != nil {
		fmt.Println("打开excel文件错误:", filename, err)
		return err
	}

	for _, sheet := range file.Sheets {
		os.MkdirAll(*cppDir, os.ModeDir)
		protoname := filepath.Join(*cppDir, sheet.Name+".proto")
		fields := genProto(filename, protoname, sheet)
		if fields == nil {
			return err
		}

		os.MkdirAll(*dataDir, os.ModeDir)
		dataname := filepath.Join(*dataDir, sheet.Name+".data")
		if err = genData(filename, dataname, fields, sheet); err != nil {
			return err
		}

		cppname := filepath.Join(*cppDir, sheet.Name)
		if err = genCpp(cppname, protoname); err != nil {
			return err
		}

		readername := filepath.Join(*cppDir, sheet.Name+"Reader.h")
		if err = genSheetReader(readername, fields, sheet); err != nil {
			return err
		}

		allReaders = append(allReaders, sheet.Name+"Reader")
	}

	return nil
}

// 生成proto文件
func genProto(filename string, protoname string, sheet *xlsx.Sheet) []*dataDesc {
	if sheet.MaxCol <= 0 || sheet.MaxRow <= 0 {
		return nil
	}

	content := "syntax = \"proto3\";\n"
	content += "package sheetcfg;\n\n"

	fields := []*dataDesc{}
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
				field.fieldType = cell.Value
			} else if nrow == 2 {
				field.comment = strings.ReplaceAll(cell.Value, "\r", " ")
				field.comment = strings.ReplaceAll(field.comment, "\n", " ")
			} else {
			}
		}

		if field.field == "" {
			continue
		}

		if idx := findDataDesc(fields, field.field); idx == -1 {
			fields = append(fields, field)
		} else {
			if fields[idx].fieldType != field.fieldType {
				fmt.Print("字段%s重复，sheet:%s|文件:%s\n", field.field, sheet.Name, filename)
				return nil
			}
			fields[idx].col = append(fields[idx].col, field.col...)
		}
	}

	// 生成结构
	content += fmt.Sprintf("// 文件生成时间: %s\n", time.Now().String())
	content += "// " + filename + " => [" + sheet.Name + "].sheet\n"
	content += "message " + sheet.Name + " {\n"
	for idx, field := range fields {
		isRepeated := false
		if strings.Contains(field.fieldType, "[]") {
			isRepeated = true
			subs := strings.Split(field.fieldType, "[]")
			field.fieldType = subs[0]
		}

		content += "  "
		if isRepeated {
			content += "repeated "
		}

		if field.fieldType == "int" {
			field.fieldType = "int64"
		}

		content += field.fieldType + " " + field.field + " = " + strconv.Itoa(idx+1) + "; // " + field.comment + "\n"
	}
	content += "}\n\n"

	// 生成结构数组
	content += "message " + sheet.Name + "Sheet {\n"
	content += "  repeated " + sheet.Name + " items = 1;\n"
	content += "}\n"

	file, err := os.Create(protoname)
	if err != nil {
		fmt.Println("生成proto文件失败:", protoname, err)
		return nil
	}

	defer file.Close()
	file.WriteString(content)
	return fields
}

// 生成数据文件
func genData(filename string, dataname string, fields []*dataDesc, sheet *xlsx.Sheet) error {
	content := fmt.Sprintf("# 文件生成时间: %s\n", time.Now().String())
	content += "# " + filename + " => [" + sheet.Name + "].sheet\n"
	content += "items: [\n"
	for nrow := 4; nrow < sheet.MaxRow; nrow++ {
		datas := map[string][]string{}
		for ncol := 0; ncol < sheet.MaxCol; ncol++ {
			idx := findDataDescByCol(fields, ncol)
			if idx == -1 {
				continue
			}

			cell, err := sheet.Cell(nrow, ncol)
			if err != nil {
				fmt.Print("打开[第%n行, 第%n列]错误，sheet:%s|文件:%s\n", nrow, ncol, sheet.Name, filename)
				return err
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
				if field.fieldType == "string" {
					content += "\"" + value + "\""
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

	file, err := os.Create(dataname)
	if err != nil {
		fmt.Println("生成data文件失败:", dataname, err)
		return err
	}

	defer file.Close()
	file.WriteString(content)
	return nil
}

// 生成cpp文件
func genCpp(cppname string, protoname string) error {
	protocExe := *protoc
	if len(protocExe) == 0 {
		if runtime.GOOS == "windows" {
			protocExe = "protoc.exe"
		} else {
			protocExe = "protoc"
		}
	}

	cmd := exec.Command(protocExe, "--cpp_out=./", protoname)
	if out, err := cmd.CombinedOutput(); err != nil {
		fmt.Println("生成cpp文件失败:", protoname)
		fmt.Println(string(out))
		return err
	}
	return nil
}

// 生成sheet_reader文件
func genSheetReader(readername string, fields []*dataDesc, sheet *xlsx.Sheet) error {
	_, err := os.Lstat(readername)
	if !os.IsNotExist(err) {
		return nil
	}

	content := fmt.Sprintf("// 文件首次生成于时间: %s \n", time.Now().String())
	content += "#pragma once\n\n"
	content += "#include \"sheet_reader.h\"\n"
	content += "#include \"" + sheet.Name + ".pb.h\"\n\n"

	content += "namespace sheetcfg {\n"
	content += "    // 索引类型\n"
	content += fmt.Sprintf("    using %sKey=SheetKey<>;\n", sheet.Name)
	content += fmt.Sprintf("    // 模板参数分别为：索引类型，自定义结构类型，默认类型(proto生成), proto表类型\n")
	content += fmt.Sprintf("    using %sBaseReader=SheetReader<%s, %s, %s, %s>;\n\n",
		sheet.Name,
		sheet.Name+"Key",
		sheet.Name,
		sheet.Name,
		sheet.Name+"Sheet")
	content += fmt.Sprintf("    struct %sReader : public %sBaseReader {\n", sheet.Name, sheet.Name)
	content += "    protected:\n"
	content += "        //解析器实现\n"
	content += fmt.Sprintf("        bool parser(%s& key, %s& newitem, %s& item) {\n",
		sheet.Name+"Key",
		sheet.Name,
		sheet.Name)
	content += "            newitem = item;\n"
	content += "            key.key1 = newitem.id();\n"
	content += "            return true;\n"
	content += "        }\n"
	content += "    };\n"
	content += "}"

	file, err := os.Create(readername)
	if err != nil {
		fmt.Println("生成reader文件失败:", readername, err)
		return err
	}

	defer file.Close()
	file.WriteString(content)
	return nil
}

// 生成reader变量
func genReaderVariable(names []string) error {
	//fmt.Println(names, len(names))

	headers := fmt.Sprintf("// 文件生成时间: %s\n", time.Now().String())
	headers += "#pragma once\n\n"
	for _, name := range names {
		headers += fmt.Sprintf("#include \"%s.h\"\n", name)
	}

	headers += "\nnamespace sheetcfg {\n"
	for _, name := range names {
		headers += fmt.Sprintf("    extern %s g%s;\n", name, name)
	}
	headers += "    bool LoadAllReader(const std::string&);\n"
	headers += "}\n"

	headerFile, err := os.Create(filepath.Join(*cppDir, "AllSheetReader.h"))
	if err != nil {
		fmt.Println("生成SheetReader.h文件错误:", err)
		return err
	}

	defer headerFile.Close()
	headerFile.WriteString(headers)

	////////////////////////////////////////////////

	cpps := fmt.Sprintf("// 文件生成时间: %s\n", time.Now().String())
	cpps += "#include \"AllSheetReader.h\"\n\n"
	cpps += "namespace sheetcfg {\n"
	for _, name := range names {
		cpps += fmt.Sprintf("   %s %s;\n", name, "g"+name)
	}
	cpps += "\n"
	cpps += "    bool LoadAllReader(const std::string& dir) {\n"
	for _, name := range names {
		dataname := fmt.Sprintf("(dir+\"/%s.data\").c_str()", name[0:len(name)-6])
		cpps += fmt.Sprintf("       if (%s.Load(%s) == false) return false;\n", "g"+name, dataname)
	}
	cpps += "       return true;"
	cpps += "    }\n"
	cpps += "}"

	cppFile, err := os.Create(filepath.Join(*cppDir, "AllSheetReader.cpp"))
	if err != nil {
		fmt.Println("生成SheetReader.cpp文件错误:", err)
		return err
	}

	defer cppFile.Close()
	cppFile.WriteString(cpps)
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
