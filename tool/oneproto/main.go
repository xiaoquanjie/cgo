package main

import (
	"flag"
	"fmt"
	"github.com/jhump/protoreflect/desc"
	"oneproto/dyproto"
	"os"
	"strings"
)

var (
	// proto文件所在的目录
	protoDir = flag.String("proto", "./", "proto files directory")
	// 文件的输出目录
	outFile = flag.String("out", "./oneproto.proto", "output files")
)

func main() {
	flag.Parse()

	parser := dyproto.Load(*protoDir)
	if parser == nil {
		fmt.Println("failed to parse proto directory:", *protoDir)
		return
	}

	data := ""
	for _, fdesc := range parser.FileDesc {
		if len(data) == 0 {
			data += "syntax = \"proto3\";\n"
			data += fmt.Sprintf("package %s;\n", fdesc.GetPackage())
			data += "import \"google/protobuf/any.proto\";\n"
			data += "import \"google/protobuf/timestamp.proto\";"
			data += fmt.Sprintf("option go_package = \"./%s;%s\";\n", fdesc.GetPackage(), fdesc.GetPackage())
		}

		for _, service := range fdesc.GetServices() {
			data += fmt.Sprintf("\nservice %s {\n", service.GetName())
			for _, method := range service.GetMethods() {
				request := method.GetInputType().GetName()
				respond := method.GetOutputType().GetName()

				if method.IsServerStreaming() && method.IsClientStreaming() {
					data += fmt.Sprintf("%s rpc %s(stream %s) returns (stream %s) {}\n", getIndent(2), method.GetName(), request, respond)
				} else if method.IsServerStreaming() {
					data += fmt.Sprintf("%s rpc %s(%s) returns (stream %s) {}\n", getIndent(2), method.GetName(), request, respond)
				} else if method.IsClientStreaming() {
					data += fmt.Sprintf("%s rpc %s(stream %s) returns (%s) {}\n", getIndent(2), method.GetName(), request, respond)
				} else {
					data += fmt.Sprintf("%s rpc %s(%s) returns (%s) {}\n", getIndent(2), method.GetName(), request, respond)
				}
			}
			data += "}\n"
		}

		for _, enum := range fdesc.GetEnumTypes() {
			data += fmt.Sprintf("\nenum %s {\n", enum.GetName())
			for _, value := range enum.GetValues() {
				data += fmt.Sprintf("%s %s = %d;\n", getIndent(2), value.GetName(), value.GetNumber())
			}
			data += "}\n"
		}

		podTypeName := func(t string) string {
			t = strings.Replace(t, "TYPE_", "", 1)
			t = strings.ToLower(t)
			return t
		}

		parseTypeName := func(desc *desc.FieldDescriptor) string {
			if desc.GetMessageType() != nil {
				typeDesc := desc.GetMessageType().GetName()
				if typeDesc == "Any" {
					typeDesc = "google.protobuf.Any"
				} else if typeDesc == "Timestamp" {
					typeDesc = "google.protobuf.Timestamp"
				}
				return typeDesc
			} else if desc.GetEnumType() != nil {
				return desc.GetEnumType().GetName()
			} else {
				return podTypeName(desc.GetType().String())
			}
		}

		for _, stu := range fdesc.GetMessageTypes() {
			data += fmt.Sprintf("\nmessage %s {\n", stu.GetName())
			for _, filed := range stu.GetFields() {
				typeDesc := ""
				if filed.IsMap() {
					key := parseTypeName(filed.GetMapKeyType())
					val := parseTypeName(filed.GetMapValueType())
					typeDesc = fmt.Sprintf("map<%s, %s>", key, val)
				} else if filed.IsRepeated() {
					typeDesc = "repeated " + parseTypeName(filed)
				} else {
					typeDesc = parseTypeName(filed)
				}

				data += fmt.Sprintf("%s %s %s = %d;\n", getIndent(2), typeDesc, filed.GetName(), filed.GetNumber())
			}
			data += "}\n"

		}
	}

	writeOutFile(data)
}

func writeOutFile(data string) {
	fDst, err := os.Create(*outFile)
	if err != nil {
		fmt.Printf("failed to generate file: %s, %s \n", *outFile, err.Error())
		return
	}

	defer fDst.Close()
	fDst.WriteString(data)
}

func getIndent(num int) string {
	i := ""
	for idx := 0; idx < num; idx++ {
		i += " "
	}
	return i
}
