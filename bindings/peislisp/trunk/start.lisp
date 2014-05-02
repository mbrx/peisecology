(in-package "COMMON-LISP-USER")

(setq ext::*complain-about-illegal-switches* nil)

;; Delete the command-line arguments that loaded this file
(pop lisp::lisp-command-line-list)
(do ((opt (pop lisp::lisp-command-line-list)
          (pop lisp::lisp-command-line-list)))
    ((char/= (schar opt 0) #\-)))

(ext::process-command-strings)
(setq ext::*complain-about-illegal-switches* t)

;; Allow a #! line on source files
(set-dispatch-macro-character #\# #\!
  (lambda (stream bang arg)
    (declare (ignore bang arg))
    (read-line stream)
    (values)))

(let ((file (first lisp::lisp-command-line-list)))
  (unwind-protect
      (when file (load file))
    (ext:quit)))
