#!/bin/bash
svn info $1 2>/dev/null | grep -E 'Revision|Changed Date'
