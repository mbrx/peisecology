#!/usr/local/bin/runlisp
;;; template.lisp
;; This is an example file on how to use the PEIS-lisp interface 
;; while creating a "stand-alone" executable script

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


;; TODO - place your own commandline interpretation here 
;; and remove from the argument list given by lisp::lisp-command-line-list

(load "/usr/local/peislisp/peislisp.lisp")
(peisk::initialize lisp::lisp-command-line-list)

(peisk::setTuple :key "do-quit" :value nil)
(peisk::setTuple :key "do-break" :value nil)

(defun run-me() 
  (loop

   (when (and (peisk::gettuple :key "do-quit") (peisk::tuplevalue)) (quit))
   (when (and (peisk::gettuple :key "do-break")  (peisk::tuplevalue)) (break))
   
   (sleep 1.0)
   ;; Do something repeatedly
   (format t "I'm a little teapot, short and stout~%")
   (format t "here is my handle, here is my spout~%")	 
   ))

(run-me)

  
