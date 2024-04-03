package main

import (
	"cogrpc/dyproto"
	"flag"
	"fmt"
	"github.com/jhump/protoreflect/desc"
	"os"
	"path"
	"path/filepath"
)

var (
	// proto文件所在的目录
	protoDir = flag.String("proto", "./client", "proto files directory")
	// 文件的输出目录
	outDir = flag.String("out", "./client", "output files directory")
	// 引用头文件的目录
	protoInclude = flag.String("include", "", "proto files include directory")
	// 客户端的命名空间
	cNameSpace = flag.String("cnamespace", "", "client name space")
	// 服务器的命名空间
	sNameSapce = flag.String("snamespace", "", "server name space")
)

func main() {
	flag.Parse()

	parser := dyproto.Load(*protoDir)
	if parser == nil {
		return
	}

	for _, v := range parser.FileDesc {
		genCppClient(v, *outDir)
		genCppServer(v, *outDir)
	}
}

func genCppClient(fDesc *desc.FileDescriptor, dst string) []string {
	pkg := fDesc.GetPackage()
	service := fDesc.GetServices()
	if len(service) == 0 {
		return nil
	}

	retVal := make([]string, 0)
	fileName := fDesc.GetName()
	fileName = fileName[:len(fileName)-len(filepath.Ext(fileName))]

	retVal = append(retVal, fileName+"_client")

	data := "// \n"
	data += "// 此文件由工具自动生成的，请务修改 \n"
	data += "// Tools built from xiaoqj \n"
	data += "// \n\n"
	data += "#pragma once \n\n"

	if len(*protoInclude) == 0 {
		data += fmt.Sprintf("#include \"%s.grpc.pb.h\" \n", fileName)
	} else {
		data += fmt.Sprintf("#include \"%s/%s.grpc.pb.h\" \n", *protoInclude, fileName)
	}

	data += "#include \"cogrpc/cogrpc.h\" \n\n"

	indent := 0
	if len(*cNameSpace) != 0 {
		data += fmt.Sprintf("namespace %s {\n", *cNameSpace)
		indent = 4
	}

	for _, s := range service {
		indent2 := indent

		className := fmt.Sprintf("%sClient", s.GetName())
		retVal = append(retVal, className)

		data += fmt.Sprintf("%sclass %s : public cogrpc::Client<%s::%s> {\n", space(indent2), className, pkg, s.GetName())
		data += fmt.Sprintf("%spublic: \n", space(indent2))

		methodData := ""

		for _, m := range s.GetMethods() {
			indent3 := indent2 + 4

			request := pkg + "::" + m.GetInputType().GetName()
			response := pkg + "::" + m.GetOutputType().GetName()
			//fmt.Println(request, response)

			if m.IsClientStreaming() && m.IsServerStreaming() {
				// 双流
				methodData += fmt.Sprintf("%s GRPC_CLIENT_BS_METHOD(%s, %s, %s);\n\n", space(indent3), m.GetName(), request, response)
			} else if m.IsClientStreaming() {
				// 客户端流
				methodData += fmt.Sprintf("%s GRPC_CLIENT_CS_METHOD(%s, %s, %s);\n\n", space(indent3), m.GetName(), request, response)
			} else if m.IsServerStreaming() {
				// 服务器流
				methodData += fmt.Sprintf("%s GRPC_CLIENT_SS_METHOD(%s, %s, %s);\n\n", space(indent3), m.GetName(), request, response)
			} else {
				// 一元
				methodData += fmt.Sprintf("%s GRPC_CLIENT_UNARY_METHOD(%s, %s, %s);\n\n", space(indent3), m.GetName(), request, response)
			}
		}

		data += methodData
		data += fmt.Sprintf("%s};\n\n", space(indent))
	}

	if len(*cNameSpace) != 0 {
		data += fmt.Sprintf("}\n")
	}

	outputFile(data, fileName+"_client", dst)
	return retVal
}

func genCppServer(fDesc *desc.FileDescriptor, dst string) {
	pkg := fDesc.GetPackage()
	service := fDesc.GetServices()
	if len(service) == 0 {
		return
	}

	fileName := fDesc.GetName()
	fileName = fileName[:len(fileName)-len(filepath.Ext(fileName))]

	data := "// \n"
	data += "// 此文件由工具自动生成的，请务修改 \n"
	data += "// Tools built from xiaoqj \n"
	data += "// \n\n"
	data += "#pragma once \n\n"

	if len(*protoInclude) == 0 {
		data += fmt.Sprintf("#include \"%s.grpc.pb.h\" \n", fileName)
	} else {
		data += fmt.Sprintf("#include \"%s/%s.grpc.pb.h\" \n", *protoInclude, fileName)
	}

	data += "#include \"cogrpc/cogrpc.h\" \n\n"

	indent := 0
	if len(*sNameSapce) != 0 {
		data += fmt.Sprintf("namespace %s {\n", *sNameSapce)
		indent = 4
	}

	for _, s := range service {
		indent2 := indent

		className := fmt.Sprintf("%sServer", s.GetName())
		data += fmt.Sprintf("%sclass %s : public cogrpc::Server<%s::%s> {\n", space(indent2), className, pkg, s.GetName())
		data += fmt.Sprintf("%spublic: \n", space(indent2))

		methodData := ""
		indent3 := indent2 + 4
		methodData += fmt.Sprintf("%svoid InitMethod() override { \n", space(indent3))
		for _, m := range s.GetMethods() {
			indent4 := indent3 + 4
			request := pkg + "::" + m.GetInputType().GetName()
			response := pkg + "::" + m.GetOutputType().GetName()

			if m.IsClientStreaming() && m.IsServerStreaming() {
				// 双流
				methodData += fmt.Sprintf("%s GRPC_SRV_BS_METHOD(%s, %s, %s);\n", space(indent4), m.GetName(), request, response)
			} else if m.IsClientStreaming() {
				// 客户端流
				methodData += fmt.Sprintf("%s GRPC_SRV_CS_METHOD(%s, %s, %s);\n", space(indent4), m.GetName(), request, response)
			} else if m.IsServerStreaming() {
				// 服务器流
				methodData += fmt.Sprintf("%s GRPC_SRV_SS_METHOD(%s, %s, %s);\n", space(indent4), m.GetName(), request, response)
			} else {
				// 一元
				methodData += fmt.Sprintf("%s GRPC_SRV_UNARY_METHOD(%s, %s, %s);\n", space(indent4), m.GetName(), request, response)
			}
		}
		methodData += fmt.Sprintf("%s}\n", space(indent3))

		for _, m := range s.GetMethods() {
			indent4 := indent3 + 4
			request := pkg + "::" + m.GetInputType().GetName()
			response := pkg + "::" + m.GetOutputType().GetName()

			if m.IsClientStreaming() && m.IsServerStreaming() {
				// 双流
				methodData += fmt.Sprintf("%s::grpc::Status %s(::grpc::ServerContext *ctx, GRPC_SRV_RW(%s, %s) *rw) {\n", space(indent3), m.GetName(), request, response)
				methodData += fmt.Sprintf("%sreturn ::grpc::Status::OK;\n%s}\n\n", space(indent4), space(indent3))
			} else if m.IsClientStreaming() {
				// 客户端流
				methodData += fmt.Sprintf("%s::grpc::Status %s(::grpc::ServerContext *ctx, GRPC_SRV_READER(%s, %s) *reader, %s *rsp) {\n", space(indent3), m.GetName(), request, response, response)
				methodData += fmt.Sprintf("%sreturn ::grpc::Status::OK;\n%s}\n\n", space(indent4), space(indent3))
			} else if m.IsServerStreaming() {
				// 服务器流
				methodData += fmt.Sprintf("%s::grpc::Status %s(::grpc::ServerContext *ctx, const %s *req, GRPC_SRV_WRITER(%s, %s) *writer) {\n", space(indent3), m.GetName(), request, request, response)
				methodData += fmt.Sprintf("%sreturn ::grpc::Status::OK;\n%s}\n\n", space(indent4), space(indent3))
			} else {
				// 一元
				methodData += fmt.Sprintf("%s::grpc::Status %s(::grpc::ServerContext *ctx, const %s *req, %s *rsp) {\n", space(indent3), m.GetName(), request, response)
				methodData += fmt.Sprintf("%sreturn ::grpc::Status::OK;\n%s}\n\n", space(indent4), space(indent3))
			}
		}

		data += methodData
		data += fmt.Sprintf("%s};\n\n", space(indent))
	}

	if len(*sNameSapce) != 0 {
		data += fmt.Sprintf("}\n")
	}

	outputFile(data, fileName+"_server", dst)
}

func outputFile(data string, name string, dst string) {
	dstFile := path.Join(dst, name+".hpp")

	fDst, err := os.Create(dstFile)
	if err != nil {
		fmt.Printf("failed to generate file: %s, %s \n", dstFile, err.Error())
		return
	}

	defer fDst.Close()
	fDst.WriteString(data)
}

func space(c int) string {
	d := ""
	for i := 0; i < c; i++ {
		d += " "
	}
	return d
}
