;;; peislisp.lisp
;; To use this interface to the PEIS-kernel do (load "/usr/local/peislisp/peislisp.lisp")

#|
    Copyright (C) 2005 - 2006  Mathias Broxvall

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA
|#


(if (not (find-package 'peisk)) (make-package 'peisk))
(in-package peisk)

(import '(c-call:int c-call:void c-call:float c-call:double c-call:c-string))

(setq libs `("-L/usr/X11R6/lib"	"-L/usr/local/lib" "-lc" "-lm" "-lpthread" "-lpeiskernel" "-lpeiskernel_mt"))
(setq o-files `("/usr/local/peislisp/peislisp.o"))

;; Handle MacOS linking
(when (member :darwin *features*)
  (setq o-files (append o-files '("/usr/lib/dylib1.o")))
  (setq libs (append libs '("-dylib")))
)

(ext:load-foreign o-files :libraries libs)

;; Used internally to force something to be a proper key
(defun coerce-key (k) (if (stringp k) k (format nil "~a" k)))
;; Used internally to force something to be a proper value
(defun coerce-value (v) (if (stringp v) v (format nil "~a" v)))


;; Function that initializes the peiskernel interface. Should be called exactly once.
(alien:def-alien-routine ("peislisp_initialize" initialize0) void
  (str c-string :in))
(defun initialize (str)
  (cond
    ((listp str)
     (initialize0 (apply #'concatenate 'string (mapcar (lambda (x) (format nil "~a " x)) str))))
    ((stringp str)
     (initialize0 str))
    (t 
     (error "Bad argument types given to peisk::initilalize. It should be a _list_ of args"))))


(alien:def-alien-routine ("peiskmt_shutdown" shutdown) void)

(alien:def-alien-routine ("peiskmt_gettime" gettime) int)
(alien:def-alien-routine ("peiskmt_gettimef" gettimef) double)
(alien:def-alien-routine ("peiskmt_connect" connect) int (url c-string :in))
(alien:def-alien-routine ("peiskmt_autoConnect" connect) void (url c-string :in))
(alien:def-alien-routine ("peiskmt_peisid" peisid) int)

(alien:def-alien-routine ("peiskmt_unsubscribe" unsubscribe0) void (handle int :in))
(defun unsubscribe (handle)
  (peisk::unsubscribe0 handle))

(alien:def-alien-routine ("peiskmt_subscribe" subscribe0) int (key c-string  :in) (owner int :in))
(defun subscribe (&key (key nil) (owner (peisid)))
  (peisk::subscribe0 (coerce-key key) owner))

(alien:def-alien-routine ("peislisp_nextExpire" next-expire0) void (time double :in))
(defun next-expire(time) (next-expire0 (coerce time 'double-float)))
(alien:def-alien-routine ("peislisp_nextUser" next-user0) void (time double :in))
(defun next-user(time) (next-user0 (coerce time 'double-float)))

(alien:def-alien-routine ("peislisp_setTuple" setTuple0) void
  (key c-string :in)
  (value c-string :in)
  (owner int :in))


(defun setTuple (&key (key nil) (value nil) (owner nil) (expire nil) (user nil))
  (let ((str (format nil "~a" value)))
    (when expire
      (next-expire expire))
    (when user
      (next-user user))
    (setTuple0 (coerce-key key) str (if owner owner (peisid)))
    ))
    
(alien:def-alien-routine ("peislisp_tupleValue" tupleValue0) c-string)
(defun tupleValue (&key (parse T))
  (if parse 
      ;; Note, if parsing fails - then we return a string with the content instead and as SECOND value 'error
      ;; otherwise, second value is always nil
      (handler-case (values (read-from-string (tupleValue0)) nil)
		    (END-OF-FILE () (values (tupleValue0) 'error)))
    (values (tupleValue0) nil)))

(alien:def-alien-routine ("peislisp_tupleOwner" tupleOwner) int)
;; Gives the key, as a string, of the latest tuple
(alien:def-alien-routine ("peislisp_tupleKey" tupleKey) c-string)

(alien:def-alien-routine ("peislisp_tupleWriteTS" tuple-ts-write) double)
(alien:def-alien-routine ("peislisp_tupleExpireTS" tuple-ts-expire) double)
(alien:def-alien-routine ("peislisp_tupleUserTS" tuple-ts-user) double)

;;DEPRACATED - does not follow naming standard! (alien:def-alien-routine ("peiskmt_peisid" peiskmt_peisid) int)
(alien:def-alien-routine ("peiskmt_peisid" peisid) int)
;;Backwards comaptible since we made the peiskmt_peisid function above depracated
(defun peiskmt_peisid () (peisid))

;; Creates a LISP list from a string key, using NULL for wildcards. */
(alien:def-alien-routine ("peislisp_key2list" key2list0) c-string
  (key c-string :in))
(defun key2list(key) 
  (handler-case (read-from-string (key2list0 (coerce-key key)) nil)
    (END-OF-FILE () nil)))
;; Converts a LISP list to a C-string key, treating NULL as wildcards. */
(defun list2key (l) 
  (cond
    ((null l) "")
    ((null (rest l))
     (if (null (first l)) "*" (format nil "~a" (first l)))
     )
    (T
     (format nil "~a.~a" (if (null (first l)) "*" (first l)) (list2key (rest l))))
    ))

;; Finds all tuples matching key/owner. Use nextResult to iterate over the results. Returns number of found results. */
(defun findTuples(&key (key "*") (owner -1) (old nil))
  (findTuples0 (coerce-key key) owner (if old 1 0)))
(alien:def-alien-routine ("peislisp_findTuples" findTuples0) int
  (key c-string :in)
  (owner int :in)
  (old int :in))
;; Steps to the next result tuple, if any. Returns true if ok, false if no more results available. Use tupleValue, .. to access the actual result */
(alien:def-alien-routine ("peislisp_nextResult" nextResult0) int)
(defun nextResult () (if (= (nextResult0) 0) nil T))


(defconstant flag_oldval 1)
(defconstant flag_blocking 2)
(alien:def-alien-routine ("peislisp_getTuple" getTuple0) int
  (key c-string :in)
  (owner int :in)
  (flags int :in))
(defun getTuple (&key (key nil) (owner (peisid)) (old nil) (blocking nil))
  (/= 0 (getTuple0 (if (stringp key) key (format nil "~a" key)) owner (+ 0 (if old flag_oldval 0) (if blocking flag_blocking 0)))))

(alien:def-alien-routine ("peislisp_getTuples_reset" getTuples_reset) void)
(alien:def-alien-routine ("peislisp_getTuples" getTuples0) int
  (key c-string :in)
  (flags int :in))

(defun getTuples (&key (key nil) (old nil) (blocking nil))
  (/= 0 (getTuples0 (if (stringp key) key (format nil "~a" key)) (+ 0 (if old flag_oldval 0) (if blocking flag_blocking 0)))))





;;
;; META TUPLES
;;
;; These routines all handle subscribing to, reading from and writing to meta tuples
;;
(alien:def-alien-routine ("peislisp_subscribeIndirectTuple" subscribeIndirect0) int
			 (key c-string :in) (owner int :in))
(defun subscribeIndirect (&key (key nil) (owner (peisid)))
  (peisk::subscribeIndirect0 (coerce-key key) owner))
(alien:def-alien-routine ("peislisp_getIndirectTuple" getIndirectTuple0) int
			 (key c-string :in) (owner int :in) (flags int :in))
(defun getIndirectTuple (&key (key nil) (owner (peisid)) (old nil) (blocking nil))
  (/= 0 (getIndirectTuple0 (coerce-key key) owner  (+ 0 (if old flag_oldval 0) (if blocking flag_blocking 0)))))

(alien:def-alien-routine ("peislisp_setIndirectStringTuple" setIndirectTuple0) int
			 (key c-string :in) (owner int :in) (value c-string :in))
(defun setIndirectTuple (&key (key nil) (value nil) (owner (peisid)) (expire nil) (user nil))
  (let ((str (format nil "~a" value)))
    (when expire
      (next-expire expire))
    (when user
      (next-user user))
    (setIndirectTuple0 (coerce-key key) (if owner owner (peisid)) str)
    ))

(alien:def-alien-routine ("peiskmt_isIndirectTuple" isIndirectTuple0) int
			 (meta-key c-string :in) (meta-owner int :in))
(defun isIndirectTuple(&key (key nil) (owner (peisid)))
  (/= 0 (peisk::isIndirectTuple0 (coerce-key key) owner)))

(alien:def-alien-routine ("peiskmt_writeMetaTuple" writeMetaTuple0) int
			 (meta-owner int :in) (meta-key c-string :in) (real-owner int :in) (real-key c-string :in))
(defun writeMetaTuple (&key (meta-key nil) (meta-owner (peisk::peisid)) 
			    (real-key nil) (real-owner (peisk::peisid)))
  (peisk::writeMetaTuple0 meta-owner (peisk::coerce-key meta-key) real-owner (peisk::coerce-key real-key)))





;; Create a special form for iterating over tuples from all owners.
;; usage: (doTuples (value-var owner-var :key "mykey" :blocking T :old T) BODY ...)
;; or: (doTuples ((value-var :parse nil) owner-var :key "mykey" :blocking :old) BODY ...)
;; the BODY is called with "value-var" bound to the value and "owner-var" bound to the owner for ever tuple found

(defmacro doTuples (form &rest body)
  `(progn
    (peisk::findTuples ,@(rest (rest form)))
    (loop
     (if (not (peisk::nextResult))
	 (return))
     (let ((,(if (listp (first form)) (first (first form)) (first form)) 
	    (peisk::tupleValue ,@(if (listp (first form)) (rest (first form)) '())))
	   (,(second form) (peisk::tupleOwner)))
       ,@body)
     )))
       

(defmacro doTuples-old (form  &rest body)
  `(progn
    (peisk::getTuples_reset)    
    (loop
     (if (peisk::getTuples ,@(rest (rest form)))
	 (let ((,(if (listp (first form)) (first (first form)) (first form)) 
		(peisk::tupleValue ,@(if (listp (first form)) (rest (first form)) '())))
	       (,(second form) (peisk::tupleOwner)))
	   ,@body)
	 (return)
	 ))
    ))

;; Example use
;; (peisk::dotuples (Val Own :key (peisk::list2key '(kernel NIL))) (format t "Tuple: ~a ~a~%" (peisk::tupleKey) (peisk::key2list (peisk::tupleKey))))

;; Ask Mathias if this is equivalent as the previous
(defmacro doTuplesMarco (form &body body)
  `(progn
    (format t "dentro il doTuples~%")
    (peisk::getTuples_reset)    
    (loop
     (format t "dentro il loop del doTuples~%")
     (if (peisk::getTuples ,@(rest (rest form)))
	 (let ((,(first form) (peisk::tupleValue))
	       (,(second form) (peisk::tupleOwner)))
	   ,@body)
	 (return)
	 ))
    ))



;; Utility function to setup a subscription to a meta tuple and to make sure 
;; that it exists in the tuplespace. This later stage helps users to see the 
;; existance of the tuple since it will then appear in the tupleviewer
(defun uses-meta-tuple (&key (key nil))
  (when key
    (peisk::subscribeIndirect :key key)
    (if (not (peisk::getTuple :key key :old T))
	(peisk::writeMetaTuple :meta-key key :real-owner -1 :real-key nil))
    ))
