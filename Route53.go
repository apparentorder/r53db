package main

import (
	// #include <stdbool.h>
	// #include "cgo_functions.h"
	// #include "dns.h"
	// #include "fdw.h"
	"C"

	"context"
	"fmt"
	"strings"

	"github.com/aws/aws-sdk-go-v2/service/route53"
)

//export r53dbGoBeginScan
func r53dbGoBeginScan(scanState *C.char, hosted_zone_id_c *C.char) {
	hosted_zone_id := C.GoString(hosted_zone_id_c)
	StoreDNSResults(scanState, hosted_zone_id)
}

//export r53dbGoEndScan
func r53dbGoEndScan() {
	// NOP
}

//export r53dbGoIterateScan
func r53dbGoIterateScan() {
	// NOP, handled in C
}

func rrSetFromRow(row *C.r53dbDNSRR) *route53.ResourceRecordSet {
	if row == nil {
		return nil
	}

	r53rr := route53.ResourceRecordSet{
		Name: GoCharStringPtr(row.name),
		Type: route53.RRType(C.GoString(row._type)),
	}

	if row.at_dns_name == nil {
		r53rr.TTL = GoInt64Ptr(row.ttl)
		r53rr.ResourceRecords = []route53.ResourceRecord{
			route53.ResourceRecord{
				Value: GoCharStringPtr(row.data),
			},
		}
	} else {
		r53rr.AliasTarget = &route53.AliasTarget{
			EvaluateTargetHealth: GoBoolPtr(bool(row.at_evaluate_target_health)),
			DNSName: GoCharStringPtr(row.at_dns_name),
			HostedZoneId: GoCharStringPtr(row.at_hosted_zone_id),
		}
	}

	return &r53rr
}

func rowsFromRRSet(rrset *route53.ResourceRecordSet) []*C.r53dbDNSRR {
	var rows []*C.r53dbDNSRR

	if rrset.AliasTarget != nil {
		row := C.r53dbNewDNSRR()
		row.name = C.CString(*rrset.Name)
		row._type = C.CString(string(rrset.Type))
		row.at_dns_name = C.CString(*(*rrset.AliasTarget).DNSName)
		row.at_hosted_zone_id = C.CString(*(*rrset.AliasTarget).HostedZoneId)
		row.at_evaluate_target_health = (C.bool) (*(*rrset.AliasTarget).EvaluateTargetHealth)
		rows = append(rows, row)
		return rows
	}

	for _, rr := range rrset.ResourceRecords {
		row := C.r53dbNewDNSRR()
		row.name = C.CString(*rrset.Name)
		row._type = C.CString(string(rrset.Type))
		row.ttl = (C.uint32_t) (int32(*rrset.TTL))
		row.data = C.CString(*rr.Value)
		rows = append(rows, row)
	}

	return rows
}


func getExistingRRSet(rname string, rtype route53.RRType, hosted_zone_id string) *route53.ResourceRecordSet {
	if !strings.HasSuffix(rname, ".") {
		rname += "."
	}

	lhzInput := route53.ListResourceRecordSetsInput{
		HostedZoneId: &hosted_zone_id,
		StartRecordName: &rname,
		StartRecordType: rtype,// route53.RRType(rtype),
		MaxItems: GoStringPtr("1"),
	}

	lhzReq := r53.ListResourceRecordSetsRequest(&lhzInput)
	resp, err := lhzReq.Send(context.TODO())
	if err != nil {
		error("ListResourceRecordSets: " + err.Error())
		return nil;
	}

	if len(resp.ResourceRecordSets) == 0 {
		return nil;
	}

	// Route53 might return entries that are *after* what we're looking
	// for. That means there was no match.

	if *resp.ResourceRecordSets[0].Name != rname {
		return nil
	}

	if resp.ResourceRecordSets[0].Type != rtype {
		return nil
	}

	return &resp.ResourceRecordSets[0]
}

func removeRRbyIndex(r []route53.ResourceRecord, index int) []route53.ResourceRecord {
	// wtf... and also: thank you,
	// https://stackoverflow.com/questions/37334119/how-to-delete-an-element-from-a-slice-in-golang
	r[index] = r[len(r) - 1]
	return r[:len(r) - 1]
}

func mergeWithExistingRRSet(
	existingRRSet *route53.ResourceRecordSet,
	newRow *route53.ResourceRecordSet,
	oldRow *route53.ResourceRecordSet,
	hosted_zone_id string,
	op C.enum_r53dbDMLOp,
) *route53.ResourceRecordSet {
	// First, some sanity checks (none of those statements will 'return' successfully)

	if op == C.DML_INSERT {
		if existingRRSet == nil {
			return newRow
		}

		if existingRRSet.AliasTarget != nil {
			error("Cannot INSERT with AliasTarget: Name already exists")
			return nil
		}
	}

	if op == C.DML_UPDATE {
		if *newRow.Name != *oldRow.Name {
			error("r53db: RRSet Name cannot be changed in UPDATE")
			return nil
		}

		if newRow.Type != oldRow.Type {
			error("r53db: RRSet Type cannot be changed in UPDATE")
			return nil
		}
	}

	if op == C.DML_INSERT || op == C.DML_UPDATE {
		if (existingRRSet.AliasTarget == nil) != (newRow.AliasTarget == nil) {
			error("Cannot modify RRSet: Inconsistent AliasTarget use in new/existing data")
			return nil
		}

		if *existingRRSet.TTL == *newRow.TTL {
			if oldRow != nil {
				*oldRow.TTL = *newRow.TTL
			}
		} else {
			notice(fmt.Sprintf("Rows added to an existing RRSet cannot have a different TTL; " +
			"using the RRSet's TTL (%d) instead", *existingRRSet.TTL))
		}
	}

	// Sanity checks ok; perform operation...

	if op == C.DML_INSERT {
		existingRRSet.ResourceRecords = append(existingRRSet.ResourceRecords, newRow.ResourceRecords[0])
		return existingRRSet
	}

	if op == C.DML_UPDATE && existingRRSet.AliasTarget != nil {
		existingRRSet.AliasTarget.DNSName = newRow.AliasTarget.DNSName
		existingRRSet.AliasTarget.HostedZoneId = newRow.AliasTarget.HostedZoneId
		existingRRSet.AliasTarget.EvaluateTargetHealth = newRow.AliasTarget.EvaluateTargetHealth
		return existingRRSet
	}

	if op == C.DML_DELETE && existingRRSet.AliasTarget != nil {
		oldRow.AliasTarget = nil
		return oldRow
	}

	// If control arrives here, we're either UPDATE or DELETE, without an Alias target.
	// We check all existing RRs, removing and/or adding the RRs from
	// UPDATE/DELETE as appropriate.

	dataToRemove := oldRow.ResourceRecords[0].Value

	replacedRRs := []route53.ResourceRecord{}

	for _, r := range existingRRSet.ResourceRecords {
		if *r.Value != *dataToRemove {
			replacedRRs = append(replacedRRs, r)
		}
	}

	if newRow != nil {
		replacedRRs = append(replacedRRs, newRow.ResourceRecords[0])
	}

	oldRow.ResourceRecords = replacedRRs
	return oldRow
}

//export r53dbGoModifyDNSRR
func r53dbGoModifyDNSRR(cid *C.char, cNewRR *C.r53dbDNSRR, cOldRR *C.r53dbDNSRR, op C.enum_r53dbDMLOp) bool {
	hosted_zone_id := C.GoString(cid)

	newRow := rrSetFromRow(cNewRR)
	oldRow := rrSetFromRow(cOldRR)

	var rrsName string
	var rrsType route53.RRType

	if newRow != nil {
		rrsName, rrsType = *newRow.Name, newRow.Type
	} else {
		rrsName, rrsType = *oldRow.Name, oldRow.Type
	}

	existingRRSet := getExistingRRSet(rrsName, rrsType, hosted_zone_id)
	mergedRRSet := mergeWithExistingRRSet(existingRRSet, newRow, oldRow, hosted_zone_id, op)

	params := route53.ChangeResourceRecordSetsInput{
		HostedZoneId: &hosted_zone_id,
		ChangeBatch: &route53.ChangeBatch{
			Changes: []route53.Change{
				route53.Change{
					Action: "UPSERT",
					ResourceRecordSet: mergedRRSet,
				},
			},
		},
	}

	debug(fmt.Sprintf("Merged RRSet for Modify operation: %v", mergedRRSet))

	// Special case: if the RRSet is empty afterwards, Action will be DELETE;
	// but we need to provide the original RRSet, because the Route53 API checks
	// all ResourceRecords (it won't allow an empty ResourceRecords list).
	// As a hack, we use the same logic to DELETE an AliasTarget entry.
	if op == C.DML_DELETE && len(mergedRRSet.ResourceRecords) == 0 && mergedRRSet.AliasTarget == nil {
		debug(fmt.Sprintf("RRSet %s is empty -- DELETEing the whole RRSet", *mergedRRSet.Name))
		params.ChangeBatch.Changes[0].Action = "DELETE"
		params.ChangeBatch.Changes[0].ResourceRecordSet = existingRRSet
	}

	req := r53.ChangeResourceRecordSetsRequest(&params)
	_, err := req.Send(context.TODO())
	if err != nil {
		error("ChangeResourceRecordSets: " + err.Error())
		return false
	}

	return true
}

//export r53dbGoGetZones
func r53dbGoGetZones(zoneList *C.char) *C.char {
	lhzReq := r53.ListHostedZonesRequest(&route53.ListHostedZonesInput{})
	lhzp := route53.NewListHostedZonesPaginator(lhzReq)
	for lhzp.Next(context.TODO()) {
		lhzPage := lhzp.CurrentPage()
		for _, zone := range lhzPage.HostedZones {
			czone := C.r53dbNewZone()
			czone.id = C.CString(*zone.Id)
			czone.name = C.CString(*zone.Name)
			zoneList = C.r53dbStoreZone(zoneList, czone)
		}
	}

	if err := lhzp.Err(); err != nil {
		error("ListHostedZones: " + err.Error())
	}

	return zoneList
}

func StoreDNSResults(scanState *C.char, hosted_zone_id string) {
	rrReq := r53.ListResourceRecordSetsRequest(&route53.ListResourceRecordSetsInput{
		HostedZoneId: &hosted_zone_id,
	})
	rrp := route53.NewListResourceRecordSetsPaginator(rrReq)
	for rrp.Next(context.TODO()) {
		rrpage := rrp.CurrentPage()
		for _, rrset := range rrpage.ResourceRecordSets {
			for _, row := range rowsFromRRSet(&rrset) {
				C.r53dbStoreResult(scanState, row)
			}
		}
	}

	if err := rrp.Err(); err != nil {
		error("r53db: StoreDNSResults(), ListResourceRecordSets: " + err.Error())
		return
	}
}

