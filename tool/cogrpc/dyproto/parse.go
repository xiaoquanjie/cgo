package dyproto

import (
	"errors"
	"fmt"
	"github.com/jhump/protoreflect/desc"
	"github.com/jhump/protoreflect/desc/protoparse"
	"github.com/jhump/protoreflect/dynamic"
	"os"
	"path/filepath"
)

type DynamicParser struct {
	PkgName        string
	parser         *protoparse.Parser
	FileDesc       map[string]*desc.FileDescriptor
	enumDescMap    map[string]*desc.EnumDescriptor
	MsgDescMap     map[string]*desc.MessageDescriptor
	ServiceDescMap map[string]*desc.ServiceDescriptor
}

func NewDynamicParser() *DynamicParser {
	return &DynamicParser{
		parser:         &protoparse.Parser{},
		FileDesc:       make(map[string]*desc.FileDescriptor),
		enumDescMap:    make(map[string]*desc.EnumDescriptor),
		MsgDescMap:     make(map[string]*desc.MessageDescriptor),
		ServiceDescMap: make(map[string]*desc.ServiceDescriptor),
	}
}

func (d *DynamicParser) SetImportPath(paths ...string) {
	for _, p := range paths {
		d.parser.ImportPaths = append(d.parser.ImportPaths, p)
	}
}

func (d *DynamicParser) ParseFiles(files ...string) error {
	fileDesc, err := d.parser.ParseFiles(files...)
	if err != nil {
		fmt.Errorf("failed to parse file: %s", files)
		fmt.Errorf("reason: %s", err.Error())
		return err
	}

	for _, desc := range fileDesc {
		if len(d.PkgName) != 0 && d.PkgName != desc.GetPackage() {
			return errors.New("conflicting package name")
		}

		d.PkgName = desc.GetPackage()

		d.FileDesc[desc.GetName()] = desc

		for _, mDesc := range desc.GetMessageTypes() {
			d.MsgDescMap[mDesc.GetName()] = mDesc
		}

		for _, eDesc := range desc.GetEnumTypes() {
			d.enumDescMap[eDesc.GetName()] = eDesc
		}

		for _, sDesc := range desc.GetServices() {
			d.ServiceDescMap[sDesc.GetName()] = sDesc
		}
	}

	return nil
}

func (d *DynamicParser) NewMessage(name string) (*dynamic.Message, error) {
	mDesc, ok := d.MsgDescMap[name]
	if !ok {
		return nil, errors.New("message: " + name + " not exist")
	}

	msg := dynamic.NewMessage(mDesc)
	return msg, nil
}

func (d *DynamicParser) ToJson(name string, data []byte) ([]byte, error) {
	msg, err := d.NewMessage(name)
	if err != nil {
		return nil, err
	}

	err = msg.Unmarshal(data)
	if err != nil {
		fmt.Errorf("failed to unmarshal '%s' data", name)
		return nil, err
	}

	json, err := msg.MarshalJSON()
	if err != nil {
		fmt.Errorf("failed to unmarshal '%s' data to json", name)
		return nil, err
	}

	return json, nil
}

func Load(dir string) *DynamicParser {
	entries, err := os.ReadDir(dir)
	if err != nil {
		fmt.Errorf("failed to find directory: %s", dir)
		return nil
	}

	files := make([]string, 0)
	for _, entry := range entries {
		ext := filepath.Ext(entry.Name())
		if ext != ".proto" {
			continue
		}

		files = append(files, entry.Name())
	}

	d := NewDynamicParser()
	d.SetImportPath(dir)
	err = d.ParseFiles(files...)

	if err != nil {
		fmt.Errorf("load proto err: %s", err.Error())
		return nil
	}

	return d
}
