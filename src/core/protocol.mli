(*
 * Copyright (C) 2011 Mauricio Fernandez <mfp@acm.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version,
 * with the special exception on linking described in file LICENSE.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *)

(** Definitions and convenience functions for remote obigstore protocol. *)

type error =
    Internal_error
  | Closed
  | Corrupted_frame
  | Bad_request
  | Unknown_serialization
  | Unknown_keyspace
  | Deadlock
  | Inconsistent_length of int * int
  | Other of int
  | Exception of exn

type request_id = string

exception Error of error

val string_of_error : error -> string
val sync_req_id : string
val is_sync_req : string -> bool

val skip : Lwt_io.input_channel -> int -> unit Lwt.t

val read_header : Lwt_io.input_channel -> (string * int * string) Lwt.t

val write_msg :
  ?flush:bool ->
  Lwt_io.output Lwt_io.channel -> string -> Bytea.t -> unit Lwt.t

type 'a writer =
    ?buf:Bytea.t ->
    Lwt_io.output_channel -> request_id:request_id -> 'a -> unit Lwt.t

type 'a reader = Lwt_io.input_channel -> 'a Lwt.t

type backup_cursor = string

module type PAYLOAD =
sig
  val bad_request : unit writer
  val unknown_keyspace : unit writer
  val unknown_serialization : unit writer
  val internal_error : unit writer
  val deadlock : unit writer
  val return_keyspace : int writer
  val return_keyspace_maybe : int option writer
  val return_keyspace_list : string list writer
  val return_table_list : string list writer
  val return_table_size_on_disk : Int64.t writer
  val return_key_range_size_on_disk : Int64.t writer
  val return_keys : string list writer
  val return_key_count : Int64.t writer
  val return_slice : Data_model.slice writer
  val return_slice_values :
    (Data_model.key option * (Data_model.key * string option list) list)
    writer
  val return_columns :
    (Data_model.column_name * Data_model.column list) option writer
  val return_column_values : string option list writer
  val return_column : (string * Data_model.timestamp) option writer
  val return_ok : unit writer
  val return_backup_dump : (string * backup_cursor option) option writer
  val return_backup_load_result : bool writer
  val return_load_stats : Load_stats.stats writer
  val return_exist_result : bool list writer

  val read_keyspace : int reader
  val read_keyspace_maybe : int option reader
  val read_keyspace_list : string list reader
  val read_table_list : string list reader
  val read_table_size_on_disk : Int64.t reader
  val read_key_range_size_on_disk : Int64.t reader
  val read_keys : string list reader
  val read_key_count : Int64.t reader
  val read_slice : Data_model.slice reader
  val read_slice_values :
    (Data_model.key option * (Data_model.key * string option list) list)
    reader
  val read_columns :
    (Data_model.column_name * Data_model.column list) option reader
  val read_column_values : string option list reader
  val read_column : (string * Data_model.timestamp) option reader
  val read_ok : unit reader
  val read_backup_dump : (string * backup_cursor option) option reader
  val read_backup_load_result : bool reader
  val read_load_stats : Load_stats.stats reader
  val read_exist_result : bool list reader
end
