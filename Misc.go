package main

import (
	// #include "cgo_functions.h"
	// #include "dns.h"
	"C"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/aws/external"
	"github.com/aws/aws-sdk-go-v2/service/route53"
)

var awsConnected bool = false
var cfg aws.Config
var r53 *route53.Client

func awsConnect() {
	if !awsConnected {
		cfg, err := external.LoadDefaultAWSConfig()
		if err != nil {
			error("unable to load SDK config, " + err.Error())
		}
		r53 = route53.New(cfg);

		awsConnected = true
	}
}

func debug(s string) {
	C.r53dbDebug(C.CString(s))
}

func error(s string) {
	C.r53dbError(C.CString(s))
}

func notice(s string) {
	C.r53dbNotice(C.CString(s))
}

//export r53dbGoOnLoad
func r53dbGoOnLoad() {
	awsConnect()
}

func GoCharStringPtr(c *C.char) *string {
	s := C.GoString(c)
	return &s
}

func GoStringPtr(ss string) *string {
	s := ss
	return &s
}

func GoInt64Ptr(u C.uint) *int64 {
	i := int64(u)
	return &i
}

func GoBoolPtr(b bool) *bool {
	p := b
	return &p
}

func main() {
	// main() is expected, but never executed.
}

