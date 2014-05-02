#!/usr/local/bin/runlisp
;;; testAll.lisp
;; Runs a series of tests on a subset of the peislisp funcitonalities

(load "/usr/local/peislisp/peislisp.lisp")
(peisk::initialize lisp::lisp-command-line-list)

(peisk::uses-meta-tuple :key 'foobars)

(peisk::setTuple :key 'a :value 0)
(peisk::writeMetaTuple :meta-key 'm :real-key 'a)
(peisk::isIndirectTuple :key 'a)
(peisk::isIndirectTuple :key 'm)
(peisk::setIndirectTuple :key 'm :value 1)
(peisk::getIndirectTuple :key 'm)
(format T "Lookup meta: ~a~%" (peisk::tupleValue))


(peisk::writeMetaTuple :meta-key 'm :real-key "kernel.all-keys" :real-owner 6236)
(peisk::getIndirectTuple :key 'm)
(format T "Before subscribe (should be null): ~a~%" (peisk::tupleValue))
(peisk::subscribeIndirect :key 'm)

(sleep 2)
(peisk::getIndirectTuple :key 'm)
(format T "After subscribe and sleep (should be ok): ~a~%" (peisk::tupleValue))
